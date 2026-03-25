/*
 * Copyright (c) 2018 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Matteo Perin <matteo.perin.2@studenti.unipd.it
 */

#include "adr-component.h"
#include "end-device-lorawan-mac.h"
#include "lora-frame-header.h"
#include "lorawan-mac-header.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ns3
{
namespace lorawan
{

////////////////////////////////////////
// LinkAdrRequest commands management //
////////////////////////////////////////

NS_LOG_COMPONENT_DEFINE("AdrComponent");

NS_OBJECT_ENSURE_REGISTERED(AdrComponent);

TypeId
AdrComponent::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AdrComponent")
            .SetGroupName("lorawan")
            .AddConstructor<AdrComponent>()
            .SetParent<NetworkControllerComponent>()
            .AddAttribute("MultipleGwCombiningMethod",
                          "Whether to average the received power of gateways or to use the maximum",
                          EnumValue(AdrComponent::AVERAGE),
                          MakeEnumAccessor<CombiningMethod>(&AdrComponent::tpAveraging),
                          MakeEnumChecker(AdrComponent::AVERAGE,
                                          "avg",
                                          AdrComponent::MAXIMUM,
                                          "max",
                                          AdrComponent::MINIMUM,
                                          "min"))
            .AddAttribute("MultiplePacketsCombiningMethod",
                          "Whether to average SNRs from multiple packets or to use the maximum",
                          EnumValue(AdrComponent::AVERAGE),
                          MakeEnumAccessor<CombiningMethod>(&AdrComponent::historyAveraging),
                          MakeEnumChecker(AdrComponent::AVERAGE,
                                          "avg",
                                          AdrComponent::MAXIMUM,
                                          "max",
                                          AdrComponent::MINIMUM,
                                          "min"))
            .AddAttribute("HistoryRange",
                          "Number of packets to use for averaging",
                          IntegerValue(4),
                          MakeIntegerAccessor(&AdrComponent::historyRange),
                          MakeIntegerChecker<int>(0, 100))
            .AddAttribute("ChangeTransmissionPower",
                          "Whether to toggle the transmission power or not",
                          BooleanValue(true),
                          MakeBooleanAccessor(&AdrComponent::m_toggleTxPower),
                          MakeBooleanChecker())
            .AddAttribute("UseAdrLite",
                          "Enable ADR-Lite binary search mode instead of standard ADR",
                          BooleanValue(false),
                          MakeBooleanAccessor(&AdrComponent::m_useAdrLite),
                          MakeBooleanChecker())
            .AddAttribute("ChangeCodingRate",
                          "Whether ADR-Lite adjusts the coding rate (CR_k)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&AdrComponent::m_toggleCodingRate),
                          MakeBooleanChecker())
            .AddAttribute("ChangeChannel",
                          "Whether ADR-Lite adjusts the channel frequency (CF_k)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&AdrComponent::m_toggleChannel),
                          MakeBooleanChecker())
            .AddTraceSource("AdrDownlinkSent",
                           "Trace fired when an ADR LinkAdrReq command is sent to a device. "
                           "Provides: deviceAddress, oldDR, newDR, oldTxPower, newTxPower, oldCF, newCF, oldCR, newCR, algorithmType (0=std, 1=lite)",
                           MakeTraceSourceAccessor(&AdrComponent::m_adrDownlinkSent),
                           "ns3::AdrComponent::AdrDownlinkSentTracedCallback");
    return tid;
}

AdrComponent::AdrComponent()
    : m_useAdrLite(false),
      m_liteMinConfigIndex(0),
      m_liteMaxConfigIndex(0),
    m_toggleTxPower(true),
    m_toggleCodingRate(true),
    m_toggleChannel(true)
{
}

AdrComponent::~AdrComponent()
{
}

void
AdrComponent::OnReceivedPacket(Ptr<const Packet> packet,
                               Ptr<EndDeviceStatus> status,
                               Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this->GetTypeId() << packet << networkStatus);

    // We will only act just before reply, when all Gateways will have received
    // the packet, since we need their respective received power.
}

void
AdrComponent::BeforeSendingReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this << status << networkStatus);

    Ptr<Packet> myPacket = status->GetLastPacketReceivedFromDevice()->Copy();
    LorawanMacHeader mHdr;
    LoraFrameHeader fHdr;
    fHdr.SetAsUplink();
    myPacket->RemoveHeader(mHdr);
    myPacket->RemoveHeader(fHdr);

    // Log device communication parameters and last received packet info for visibility
    LoraDeviceAddress devAddr = fHdr.GetAddress();
    NS_LOG_DEBUG("AdrComponent: Preparing reply for device " << devAddr);
    NS_LOG_DEBUG("AdrComponent: Device reported SF=" << unsigned(status->GetFirstReceiveWindowSpreadingFactor())
                  << " | RX1 Freq=" << status->GetFirstReceiveWindowFrequency() << " Hz");
    Ptr<ClassAEndDeviceLorawanMac> macPtr = status->GetMac();
    if (macPtr)
    {
        NS_LOG_DEBUG("AdrComponent: Device MAC params: TxPower=" << macPtr->GetTransmissionPowerDbm()
                      << " dBm | CodingRate=" << unsigned(macPtr->GetCodingRate())
                      << " | NextTxFreq=" << macPtr->GetNextTxChannelFrequency() << " Hz");
    }

    // Last received packet info
    EndDeviceStatus::ReceivedPacketInfo lastInfo = status->GetLastReceivedPacketInfo();
    uint32_t pktSize = 0;
    uint8_t pktDr = 0;
    if (lastInfo.packet)
    {
        pktSize = lastInfo.packet->GetSize();
        LoraTag pktTag;
        Ptr<Packet> tmpPkt = lastInfo.packet->Copy();
        if (tmpPkt->PeekPacketTag(pktTag))
        {
            pktDr = pktTag.GetDataRate();
        }
    }
    NS_LOG_DEBUG("AdrComponent: Last received packet info: pkt SF=" << unsigned(lastInfo.sf)
                  << " | pkt Freq=" << lastInfo.frequencyHz << " Hz"
                  << " | DataRate=" << unsigned(pktDr)
                  << " | PacketSize=" << pktSize << " bytes"
                  << " | gwCount=" << lastInfo.gwList.size());
    for (auto&& gw : lastInfo.gwList)
    {
        NS_LOG_DEBUG("  Gateway " << gw.first << " rxPower=" << gw.second.rxPower << " dBm");
    }

    // Execute the Adaptive Data Rate (ADR) algorithm only if the request bit is set
    if (fHdr.GetAdr())
    {
        if (m_useAdrLite)
        {
            /////////////////////////////
            // ADR-Lite (binary search) //
            /////////////////////////////

            LoraDeviceAddress deviceAddress = fHdr.GetAddress();

            // Lazily build the configuration space on first use,
            // using observed payload to exclude DRs that can't
            // carry the payload + MAC command overhead
            if (m_liteConfigurations.empty())
            {
                m_liteObservedPayloadBytes = myPacket->GetSize();
                NS_LOG_INFO("ADR-Lite: Observed app payload size = "
                            << m_liteObservedPayloadBytes << " bytes");
                InitializeLiteConfigurationSpace();
            }

            DeviceAdrLiteState& state = GetDeviceLiteState(deviceAddress);

            // =========================================================
            // Step 1: Algorithm 1 - Determine optimal config
            // =========================================================
            int newConfigIndex;
            AdrLiteImplementation(&newConfigIndex, status, state);

            const LiteConfiguration& newCfg = m_liteConfigurations[newConfigIndex];
            uint8_t newSf = newCfg.sf;
            uint8_t newDr = SfToDr(newSf);
            double newTxPower = newCfg.txPowerDbm;
            uint8_t newChannel = newCfg.channelFreq;
            uint8_t newCodingRate = newCfg.codingRate;

            // =========================================================
            // Step 2: Get current device parameters for comparison
            //         (Only SF and TxPower are modifiable via LinkAdrReq)
            // =========================================================
            uint8_t currentSf = status->GetFirstReceiveWindowSpreadingFactor();
            double currentTxPower = status->GetMac()->GetTransmissionPowerDbm();
            uint8_t currentChannel = GetChannelIndexFromFrequency(status->GetLastReceivedPacketInfo().frequencyHz);
            uint8_t currentCodingRate = status->GetMac()->GetCodingRate();

            // Override TxPower if toggle is disabled
            if (!m_toggleTxPower)
            {
                newTxPower = currentTxPower;
            }

            // =========================================================
            // Step 3: Algorithm 1 — Apply kᵤ(t) to the device
            //         Send a downlink whenever the new config differs
            //         from the device's current parameters.
            //         No rate limiting: faithful to the paper.
            // =========================================================
            bool sfNeedsChange = (newSf != currentSf);
            bool tpNeedsChange = (newTxPower != currentTxPower);
            bool cfNeedsChange = (newChannel != currentChannel) && m_toggleChannel;
            bool crNeedsChange = (newCodingRate != currentCodingRate) && m_toggleCodingRate;

            if (sfNeedsChange || tpNeedsChange || cfNeedsChange || crNeedsChange)
            {
                // =========================================================
                // Step 4a: LinkAdrReq - Adjust DR, TxPower, and channels
                // =========================================================
                std::list<int> enabledChannels;
                enabledChannels.push_back(0);
                enabledChannels.push_back(1);
                enabledChannels.push_back(2);

                NS_LOG_DEBUG("ADR-Lite: Sending config for device " << deviceAddress);
                NS_LOG_DEBUG("ADR-Lite:   SF: " << (int)currentSf << " -> " << (int)newSf 
                             << " (DR" << (int)newDr << ")");
                NS_LOG_DEBUG("ADR-Lite:   TP: " << currentTxPower << " -> " << newTxPower << " dBm");
                NS_LOG_DEBUG("ADR-Lite:   CF: " << (int)currentChannel << " -> " << (int)newChannel);
                NS_LOG_DEBUG("ADR-Lite:   CR: " << (int)currentCodingRate << " -> " << (int)newCodingRate);

                status->m_reply.frameHeader.AddLinkAdrReq(
                    newDr,
                    GetTxPowerIndexLite(newTxPower),
                    enabledChannels,
                    1);  // NbTrans = 1

                // Step 4b: NewChannelReq (only if channel optimization is enabled)
                if (m_toggleChannel && cfNeedsChange)
                {
                    uint32_t newFreqHz = GetChannelFrequencyHz(newChannel);
                    status->m_reply.frameHeader.AddNewChannelReq(
                        newChannel, newFreqHz, newDr, newDr);
                    NS_LOG_DEBUG("ADR-Lite:   NewChannelReq: ch=" << (int)newChannel 
                                 << " freq=" << newFreqHz << " Hz");
                }

                // Step 4c: CodingRate (applied directly via MAC in ns-3)
                if (m_toggleCodingRate && crNeedsChange)
                {
                    Ptr<ClassAEndDeviceLorawanMac> mac = status->GetMac();
                    if (mac)
                    {
                        mac->SetCodingRate(newCodingRate);
                        NS_LOG_DEBUG("ADR-Lite:   SetCodingRate: " << (int)newCodingRate 
                                     << " (4/" << (4 + newCodingRate) << ")");
                    }
                }

                status->m_reply.frameHeader.SetAsDownlink();
                status->m_reply.macHeader.SetMType(
                    LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
                status->m_reply.needsReply = true;

                // Update last sent config
                state.lastSentConfigIndex = newConfigIndex;
                state.lastAssignedSf = newSf;
                state.lastAssignedTxPower = newTxPower;
                state.lastAssignedCF = newChannel;
                state.lastAssignedCR = newCodingRate;

                // Fire trace callback with all 4 parameters
                m_adrDownlinkSent(deviceAddress.Get(),
                                  SfToDr(currentSf), newDr,
                                  currentTxPower, newTxPower,
                                  currentChannel, newChannel,
                                  currentCodingRate, newCodingRate,
                                  1);  // 1 = ADR-Lite
            }
            else
            {
                NS_LOG_DEBUG("ADR-Lite: No parameter change for device "
                             << deviceAddress << " — skipping downlink");
            }
            
            // Always update current config index
            state.currentConfigIndex = newConfigIndex;
        }
        else
        {
            ///////////////////////////
            // Standard ADR algorithm //
            ///////////////////////////
            if (int(status->GetReceivedPacketList().size()) < historyRange)
            {
                NS_LOG_ERROR("Not enough packets received by this device ("
                             << status->GetReceivedPacketList().size()
                             << ") for the algorithm to work (need " << historyRange << ")");
            }
            else
            {
                NS_LOG_DEBUG("New Adaptive Data Rate (ADR) request");

                // Get the spreading factor used by the device
                uint8_t spreadingFactor = status->GetFirstReceiveWindowSpreadingFactor();

                // Get the device transmission power (dBm)
                double transmissionPowerDbm = status->GetMac()->GetTransmissionPowerDbm();

                // New parameters for the end-device
                uint8_t newDataRate;
                double newTxPowerDbm;

                // Adaptive Data Rate (ADR) Algorithm
                AdrImplementation(&newDataRate, &newTxPowerDbm, status);

                // Change the power back to the default if we don't want to change it
                if (!m_toggleTxPower)
                {
                    newTxPowerDbm = transmissionPowerDbm;
                }

                if (newDataRate != SfToDr(spreadingFactor) ||
                    newTxPowerDbm != transmissionPowerDbm)
                {
                    // Create a list with mandatory channel indexes
                    // Liste des canaux LoRaWAN EU868 obligatoires (0, 1, 2)
                    // Ces canaux sont toujours activés pour garantir la compatibilité avec la norme.
                    int channels[] = {0, 1, 2};
                    std::list<int> enabledChannels(
                        channels, channels + sizeof(channels) / sizeof(int));

                    // Repetitions Setting
                    const int rep = 1;

                    NS_LOG_DEBUG("Sending LinkAdrReq with DR = "
                                 << (unsigned)newDataRate << " and TP = " << newTxPowerDbm
                                 << "dBm");

                    status->m_reply.frameHeader.AddLinkAdrReq(newDataRate,
                                                              GetTxPowerIndex(newTxPowerDbm),
                                                              enabledChannels,
                                                              rep);
                    status->m_reply.frameHeader.SetAsDownlink();
                    status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);

                    status->m_reply.needsReply = true;

                    // Fire callback: Standard ADR downlink sent
                    // Parameters: deviceAddr, oldDR, newDR, oldTxPower, newTxPower, algorithmType(0=Std)
                    //
                    // Ce callback (m_adrDownlinkSent) permet de tracer chaque commande LinkAdrReq envoyée par le Network Server
                    // pour modifier les paramètres de communication (Data Rate, puissance TX) d'un end device.
                    // Il alimente le compteur NsAdrDownlinks dans les fichiers CSV de résultats, et ne concerne que les downlinks ADR.
                    m_adrDownlinkSent(devAddr.Get(),              // Device address as uint32_t
                                      SfToDr(spreadingFactor),    // Old DR
                                      newDataRate,                 // New DR
                                      transmissionPowerDbm,        // Old TxPower
                                      newTxPowerDbm,               // New TxPower
                                      0, 0,                        // CF unchanged (Standard ADR)
                                      0, 0,                        // CR unchanged (Standard ADR)
                                      0);                          // Algorithm type: 0=Standard ADR
                }
                else
                {
                    NS_LOG_DEBUG("Skipped request");
                }
            }
        }
    }
    else
    {
        // Do nothing
    }
}

void
AdrComponent::OnFailedReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this->GetTypeId() << networkStatus);

    // ADR-Lite: No special handling on failed reply per Algorithm 1.
    // The binary search naturally recovers when the next uplink is received:
    // if ru(t) != ku(t-1), the search moves toward more robust configs.
}

void
AdrComponent::AdrImplementation(uint8_t* newDataRate,
                                double* newTxPower,
                                Ptr<EndDeviceStatus> status)
{
    // Compute the maximum or median SNR, based on the boolean value historyAveraging
    double m_SNR = 0;
    switch (historyAveraging)
    {
    case AdrComponent::AVERAGE:
        m_SNR = GetAverageSNR(status->GetReceivedPacketList(), historyRange);
        break;
    case AdrComponent::MAXIMUM:
        m_SNR = GetMaxSNR(status->GetReceivedPacketList(), historyRange);
        break;
    case AdrComponent::MINIMUM:
        m_SNR = GetMinSNR(status->GetReceivedPacketList(), historyRange);
    }

    NS_LOG_DEBUG("m_SNR = " << m_SNR);

    // Get the spreading factor used by the device
    uint8_t spreadingFactor = status->GetFirstReceiveWindowSpreadingFactor();

    NS_LOG_DEBUG("SF = " << (unsigned)spreadingFactor);

    // Get the device data rate and use it to get the SNR demodulation threshold
    double req_SNR = threshold[SfToDr(spreadingFactor)];

    NS_LOG_DEBUG("Required SNR = " << req_SNR);

    // Get the device transmission power (dBm)
    double transmissionPower = status->GetMac()->GetTransmissionPowerDbm();

    NS_LOG_DEBUG("Transmission Power = " << transmissionPower);

    // Compute the SNR margin taking into consideration the SNR of
    // previously received packets
    double margin_SNR = m_SNR - req_SNR;

    NS_LOG_DEBUG("Margin = " << margin_SNR);

    // Number of steps to decrement the spreading factor (thereby increasing the data rate)
    // and the TP.
    int steps = std::floor(margin_SNR / 3);

    NS_LOG_DEBUG("steps = " << steps);

    // If the number of steps is positive (margin_SNR is positive, so its
    // decimal value is high) increment the data rate, if there are some
    // leftover steps after reaching the maximum possible data rate
    //(corresponding to the minimum spreading factor) decrement the transmission power as
    // well for the number of steps left.
    // If, on the other hand, the number of steps is negative (margin_SNR is
    // negative, so its decimal value is low) increase the transmission power
    //(note that the spreading factor is not incremented as this particular algorithm
    // expects the node itself to raise its spreading factor whenever necessary).
    while (steps > 0 && spreadingFactor > min_spreadingFactor)
    {
        spreadingFactor--;
        steps--;
        NS_LOG_DEBUG("Decreased SF by 1");
    }
    while (steps > 0 && transmissionPower > min_transmissionPower)
    {
        transmissionPower -= 2;
        steps--;
        NS_LOG_DEBUG("Decreased Ptx by 2");
    }
    while (steps < 0 && transmissionPower < max_transmissionPower)
    {
        transmissionPower += 2;
        steps++;
        NS_LOG_DEBUG("Increased Ptx by 2");
    }

    *newDataRate = SfToDr(spreadingFactor);
    *newTxPower = transmissionPower;
}

uint8_t
AdrComponent::SfToDr(uint8_t sf)
{
    switch (sf)
    {
    case 12:
        return 0;
        break;
    case 11:
        return 1;
        break;
    case 10:
        return 2;
        break;
    case 9:
        return 3;
        break;
    case 8:
        return 4;
        break;
    default:
        return 5;
        break;
    }
}

double
AdrComponent::RxPowerToSNR(double transmissionPower) const
{
    // The following conversion ignores interfering packets
    return transmissionPower + 174 - 10 * log10(B) - NF;
}

// Get the maximum received power (it considers the values in dB!)
double
AdrComponent::GetMinTxFromGateways(EndDeviceStatus::GatewayList gwList)
{
    auto it = gwList.begin();
    double min = it->second.rxPower;

    for (; it != gwList.end(); it++)
    {
        if (it->second.rxPower < min)
        {
            min = it->second.rxPower;
        }
    }

    return min;
}

// Get the maximum received power (it considers the values in dB!)
double
AdrComponent::GetMaxTxFromGateways(EndDeviceStatus::GatewayList gwList)
{
    auto it = gwList.begin();
    double max = it->second.rxPower;

    for (; it != gwList.end(); it++)
    {
        if (it->second.rxPower > max)
        {
            max = it->second.rxPower;
        }
    }

    return max;
}

// Get the maximum received power
double
AdrComponent::GetAverageTxFromGateways(EndDeviceStatus::GatewayList gwList)
{
    double sum = 0;

    for (auto it = gwList.begin(); it != gwList.end(); it++)
    {
        NS_LOG_DEBUG("Gateway at " << it->first << " has TP " << it->second.rxPower);
        sum += it->second.rxPower;
    }

    double average = sum / gwList.size();

    NS_LOG_DEBUG("TP (average) = " << average);

    return average;
}

double
AdrComponent::GetReceivedPower(EndDeviceStatus::GatewayList gwList)
{
    switch (tpAveraging)
    {
    case AdrComponent::AVERAGE:
        return GetAverageTxFromGateways(gwList);
    case AdrComponent::MAXIMUM:
        return GetMaxTxFromGateways(gwList);
    case AdrComponent::MINIMUM:
        return GetMinTxFromGateways(gwList);
    default:
        return -1;
    }
}

// TODO Make this more elegant
double
AdrComponent::GetMinSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange)
{
    double m_SNR;

    // Take elements from the list starting at the end
    auto it = packetList.rbegin();
    double min = RxPowerToSNR(GetReceivedPower(it->second.gwList));

    for (int i = 0; i < historyRange; i++, it++)
    {
        m_SNR = RxPowerToSNR(GetReceivedPower(it->second.gwList));

        NS_LOG_DEBUG("Received power: " << GetReceivedPower(it->second.gwList));
        NS_LOG_DEBUG("m_SNR = " << m_SNR);

        if (m_SNR < min)
        {
            min = m_SNR;
        }
    }

    NS_LOG_DEBUG("SNR (min) = " << min);

    return min;
}

double
AdrComponent::GetMaxSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange)
{
    double m_SNR;

    // Take elements from the list starting at the end
    auto it = packetList.rbegin();
    double max = RxPowerToSNR(GetReceivedPower(it->second.gwList));

    for (int i = 0; i < historyRange; i++, it++)
    {
        m_SNR = RxPowerToSNR(GetReceivedPower(it->second.gwList));

        NS_LOG_DEBUG("Received power: " << GetReceivedPower(it->second.gwList));
        NS_LOG_DEBUG("m_SNR = " << m_SNR);

        if (m_SNR > max)
        {
            max = m_SNR;
        }
    }

    NS_LOG_DEBUG("SNR (max) = " << max);

    return max;
}

double
AdrComponent::GetAverageSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange)
{
    double sum = 0;
    double m_SNR;

    // Take elements from the list starting at the end
    auto it = packetList.rbegin();
    for (int i = 0; i < historyRange; i++, it++)
    {
        m_SNR = RxPowerToSNR(GetReceivedPower(it->second.gwList));

        NS_LOG_DEBUG("Received power: " << GetReceivedPower(it->second.gwList));
        NS_LOG_DEBUG("m_SNR = " << m_SNR);

        sum += m_SNR;
    }

    double average = sum / historyRange;

    NS_LOG_DEBUG("SNR (average) = " << average);

    return average;
}

uint8_t
AdrComponent::GetTxPowerIndex(double txPower)
{
    NS_ASSERT_MSG(txPower >= 0 && txPower <= 14, "TxPower dBm value out of supported range");
    NS_ASSERT_MSG(fmod(txPower, 2) == 0, "Invalid TxPower value");
    return 7 - txPower / 2;
}

////////////////////////////////////////
// ADR-Lite implementation methods    //
////////////////////////////////////////

void
AdrComponent::InitializeLiteConfigurationSpace()
{
    NS_LOG_FUNCTION(this);

    m_liteConfigurations.clear();

    // ===========================================================
    // ADR-Lite Configuration Space: I_k = {SF_k, TP_k, CF_k, CR_k}
    //
    // Full 4-parameter optimization space sorted by Energy Consumption.
    // Binary search finds the optimal trade-off between energy and reliability.
    //
    // Parameters:
    //   - SF: Spreading Factor (7-12)
    //   - TP: Transmission Power {2, 4, 6, 8, 10} dBm
    //   - CF: Channel Frequency index (0, 1, 2 for EU868 mandatory)
    //   - CR: Coding Rate {1=4/5, 2=4/6, 3=4/7, 4=4/8}
    //
    // Only include configurations whose DR can carry the application payload
    // + MAC command overhead. Sorted ascending by EC.
    // ===========================================================

    // EU868 max MACPayload per DR (index = DR number)
    const std::vector<uint32_t> maxPayloadPerDr = {59, 59, 59, 123, 230, 230, 230, 230};

    // Estimate worst-case frame header size with MAC command responses
    // Normal frame header: 8 bytes (DevAddr=4 + FCtrl=1 + FCnt=2 + FPort=1)
    // With LinkAdrAns + NewChannelAns: 14 bytes
    const uint32_t macOverhead = 14;

    // Parameter ranges
    const std::vector<double> txPowerLevels = {2.0, 4.0, 6.0, 8.0, 10.0};  // 5 TP levels
    const std::vector<uint8_t> channelIndices = {0, 1, 2};                  // 3 mandatory EU868 channels
    const std::vector<uint8_t> codingRates = {1, 2, 3, 4};                  // CR 4/5, 4/6, 4/7, 4/8

    // Generate all valid configurations
    for (uint8_t sf = 7; sf <= 12; ++sf)
    {
        uint8_t dr = SfToDr(sf);
        uint32_t maxPayload = maxPayloadPerDr.at(dr);

        // Check if the observed payload + MAC overhead fits at this DR
        if (m_liteObservedPayloadBytes + macOverhead > maxPayload)
        {
            NS_LOG_INFO("ADR-Lite: Excluding SF" << (int)sf
                        << " (DR" << (int)dr << "): payload+overhead "
                        << m_liteObservedPayloadBytes + macOverhead
                        << " > maxMACPayload " << maxPayload);
            continue;
        }

        for (double tp : txPowerLevels)
        {
            // Only vary TP if m_toggleTxPower is enabled, otherwise use max power (10 dBm)
            if (!m_toggleTxPower && tp != 10.0)
            {
                continue;
            }

            for (uint8_t cf : channelIndices)
            {
                // Only vary channel if m_toggleChannel is enabled, otherwise use channel 0
                if (!m_toggleChannel && cf != 0)
                {
                    continue;
                }

                for (uint8_t cr : codingRates)
                {
                    // Only vary CR if m_toggleCodingRate is enabled, otherwise use CR=1
                    if (!m_toggleCodingRate && cr != 1)
                    {
                        continue;
                    }

                    LiteConfiguration config;
                    config.sf = sf;
                    config.txPowerDbm = tp;
                    config.channelFreq = cf;
                    config.codingRate = cr;
                    config.energyIndex = CalculateEnergyIndex(sf, tp, cr);
                    m_liteConfigurations.push_back(config);
                }
            }
        }
    }

    // Sort by energy index (ascending: most efficient first, most robust last)
    std::sort(m_liteConfigurations.begin(), m_liteConfigurations.end());

    m_liteMinConfigIndex = 0;
    m_liteMaxConfigIndex = static_cast<int>(m_liteConfigurations.size()) - 1;

    NS_LOG_INFO("ADR-Lite: Initialized |K|=" << m_liteConfigurations.size()
                << " configurations (TP=" << m_toggleTxPower
                << " CF=" << m_toggleChannel
                << " CR=" << m_toggleCodingRate << ")");
    for (size_t i = 0; i < m_liteConfigurations.size(); ++i)
    {
        NS_LOG_INFO("ADR-Lite: I_" << i 
                    << ": SF" << (int)m_liteConfigurations[i].sf
                    << " TP=" << m_liteConfigurations[i].txPowerDbm << "dBm"
                    << " CF=" << (int)m_liteConfigurations[i].channelFreq
                    << " CR=" << (int)m_liteConfigurations[i].codingRate
                    << " EC=" << m_liteConfigurations[i].energyIndex);
    }
}

double
AdrComponent::CalculateToA(uint8_t sf, int payloadBytes, uint8_t cr) const
{
    double tSymbol = std::pow(2.0, sf) / m_liteBandwidth;
    double tPreamble = (4.25 + m_litePreambleSymbols) * tSymbol;

    int H = m_liteHeaderEnabled ? 0 : 1;
    int DE = (sf >= 11) ? 1 : 0;
    int CR = cr;

    double numerator = 8.0 * payloadBytes - 4.0 * sf + 28.0 + 16.0 - 20.0 * H;
    double denominator = 4.0 * (sf - 2.0 * DE);

    int nPayload = 8 + std::max(static_cast<int>(std::ceil(numerator / denominator)) * (CR + 4), 0);
    double tPayload = nPayload * tSymbol;

    return (tPreamble + tPayload) * 1000.0; // ms
}

double
AdrComponent::CalculateEnergyIndex(uint8_t sf, double txPowerDbm, uint8_t cr) const
{
    double toA = CalculateToA(sf, m_litePayloadBytes, cr);
    double txPowerMw = std::pow(10.0, txPowerDbm / 10.0);
    return toA * txPowerMw;
}

AdrComponent::DeviceAdrLiteState&
AdrComponent::GetDeviceLiteState(LoraDeviceAddress deviceAddress)
{
    auto it = m_deviceLiteStates.find(deviceAddress);
    if (it == m_deviceLiteStates.end())
    {
        // =====================================================
        // Algorithm 1, line 8: ku(0) = |K|
        // Initialize to highest energy (most robust) config
        // =====================================================
        DeviceAdrLiteState newState;
        newState.currentConfigIndex = m_liteMaxConfigIndex;  // Start at most robust
        newState.initialized = false;
        newState.lastSentConfigIndex = -1;
        newState.awaitingAck = false;
        newState.lastAssignedSf = 12;  // Most robust SF
        newState.lastAssignedTxPower = 14;
        newState.lastAssignedCF = 0;
        newState.lastAssignedCR = 1;
        newState.consecutiveSuccesses = 0;
        newState.consecutiveFailures = 0;
        // Initialize enhanced PDR tracking fields
        newState.totalPacketsReceived = 0;
        newState.recentSuccesses = 0;
        newState.recentTotal = 0;
        newState.recoveryHoldCounter = 0;
        newState.lastOptimizationIndex = m_liteMaxConfigIndex;

        m_deviceLiteStates[deviceAddress] = newState;

        NS_LOG_DEBUG("ADR-Lite: New device " << deviceAddress
                     << " state created (starting at most robust config)");
    }
    return m_deviceLiteStates[deviceAddress];
}

bool
AdrComponent::ReceivedMatchesAssigned(Ptr<EndDeviceStatus> status,
                                      const DeviceAdrLiteState& state) const
{
    // =====================================================
    // Algorithm 1, line 12: Check if ru(t) == ku(t-1)
    // ADR-Lite only controls SF, so we compare SF only
    // =====================================================
    
    uint8_t rxSf = status->GetFirstReceiveWindowSpreadingFactor();
    const LiteConfiguration& assigned = m_liteConfigurations[state.currentConfigIndex];

    bool match = (rxSf == assigned.sf);

    NS_LOG_DEBUG("ADR-Lite [Check ru==ku]: rxSF=" << (int)rxSf 
                 << " | assigned SF=" << (int)assigned.sf 
                 << " | match=" << match);

    return match;
}

int
AdrComponent::FindConfigIndex(uint8_t sf, double txPowerDbm) const
{
    // Config space is SF-only (SF7=index 0, SF12=index 5)
    // Find the config matching this SF
    for (size_t i = 0; i < m_liteConfigurations.size(); ++i)
    {
        if (m_liteConfigurations[i].sf == sf)
        {
            return static_cast<int>(i);
        }
    }
    // Default: return highest index (most robust)
    return m_liteMaxConfigIndex;
}

int
AdrComponent::FindConfigIndexFull(uint8_t sf, double txPowerDbm, uint8_t channelIndex, uint8_t codingRate) const
{
    // =====================================================
    // Find the configuration index rᵤ(t) that best matches
    // the received packet parameters Iₖ = {SFₖ, TPₖ, CFₖ, CRₖ}
    //
    // Strategy: Find exact match, or closest by energy index
    // =====================================================
    
    int bestIndex = m_liteMaxConfigIndex;  // Default: most robust
    double bestDistance = std::numeric_limits<double>::max();
    
    for (size_t i = 0; i < m_liteConfigurations.size(); ++i)
    {
        const LiteConfiguration& cfg = m_liteConfigurations[i];
        
        // Check for exact match on all 4 parameters
        if (cfg.sf == sf &&
            std::abs(cfg.txPowerDbm - txPowerDbm) < 0.5 &&
            cfg.channelFreq == channelIndex &&
            cfg.codingRate == codingRate)
        {
            // Exact match found
            return static_cast<int>(i);
        }
        
        // Calculate distance based on weighted parameter differences
        // Primary weight on SF (most important for energy)
        double sfDiff = std::abs(static_cast<int>(cfg.sf) - static_cast<int>(sf));
        double tpDiff = std::abs(cfg.txPowerDbm - txPowerDbm) / 12.0;  // Normalize to [0,1]
        double cfDiff = (cfg.channelFreq == channelIndex) ? 0.0 : 0.1;
        double crDiff = std::abs(static_cast<int>(cfg.codingRate) - static_cast<int>(codingRate)) / 3.0;
        
        double distance = sfDiff * 10.0 + tpDiff + cfDiff + crDiff;
        
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }
    
    NS_LOG_DEBUG("FindConfigIndexFull: SF" << (int)sf << " TP=" << txPowerDbm 
                << " CF=" << (int)channelIndex << " CR=" << (int)codingRate
                << " -> index " << bestIndex);
    
    return bestIndex;
}

void
AdrComponent::AdrLiteImplementation(int* newConfigIndex, 
                                    Ptr<EndDeviceStatus> status,
                                    DeviceAdrLiteState& state)
{
    // =====================================================
    // Algorithm 1: ADR-Lite on NS (Original Implementation)
    //
    // input : kᵤ(t−1), rᵤ(t)
    // output: kᵤ(t)
    //
    // Initialization:
    //   - Iₖ = {SFₖ, TPₖ, CFₖ, CRₖ}
    //   - K = {I₁, I₂, …, I|K|} sorted ascending by EC
    //   - kᵤ(0) = |K|
    //
    // Algorithm:
    //   if rᵤ(t) = kᵤ(t−1) then
    //       minᵤ = 1
    //       maxᵤ = kᵤ(t−1)
    //   else
    //       minᵤ = kᵤ(t−1)
    //       maxᵤ = |K|
    //   end if
    //   kᵤ(t) = I₍⌊(maxᵤ + minᵤ)/2⌋₎
    //
    // Note: Paper uses 1-indexed (I₁ to I|K|)
    //       Code uses 0-indexed (I₀ to I_{|K|-1})
    //       So min=1 in paper → min=0 in code
    // =====================================================
    NS_LOG_FUNCTION(this << status);

    // --- Get rᵤ(t): config index from received packet ---
    uint8_t rxSf = status->GetFirstReceiveWindowSpreadingFactor();
    double rxTxPower = status->GetMac()->GetTransmissionPowerDbm();
    uint8_t rxCodingRate = status->GetMac()->GetCodingRate();
    uint32_t rxFreq = status->GetLastReceivedPacketInfo().frequencyHz;
    uint8_t rxChannel = GetChannelIndexFromFrequency(rxFreq);
    
    // Find the configuration index that best matches the received parameters
    int ru = FindConfigIndexFull(rxSf, rxTxPower, rxChannel, rxCodingRate);
    
    // --- Get kᵤ(t-1): previous assigned config ---
    int ku_prev = state.currentConfigIndex;

    // --- Initialization: kᵤ(0) = |K| ---
    if (!state.initialized)
    {
        // kᵤ(0) = |K| (most robust config, highest energy consumption)
        state.currentConfigIndex = m_liteMaxConfigIndex;
        state.initialized = true;
        state.totalPacketsReceived = 0;
        
        *newConfigIndex = m_liteMaxConfigIndex;
        
        const LiteConfiguration& cfg = m_liteConfigurations[m_liteMaxConfigIndex];
        NS_LOG_INFO("ADR-Lite [Init]: kᵤ(0) = |K| = " << m_liteMaxConfigIndex
                    << " | SF" << (int)cfg.sf
                    << " TP=" << cfg.txPowerDbm << "dBm"
                    << " CF=" << (int)cfg.channelFreq
                    << " CR=" << (int)cfg.codingRate);
        return;
    }

    state.totalPacketsReceived++;
    
    // =====================================================
    // Algorithm 1 Core Logic (Original - No Enhancements)
    // =====================================================
    
    int minU, maxU;
    int ku_new;
    
    if (ru == ku_prev)
    {
        // =====================================================
        // SUCCESS: rᵤ(t) = kᵤ(t−1)
        // Device is using the assigned configuration
        // → Search toward more efficient configs (lower EC)
        //
        // minᵤ = 1 (paper) → 0 (0-indexed code)
        // maxᵤ = kᵤ(t−1)
        // =====================================================
        minU = 0;                 // min = 1 in paper (1-indexed) → 0 (0-indexed)
        maxU = ku_prev;           // max = kᵤ(t-1)
        
        NS_LOG_DEBUG("ADR-Lite [Match]: rᵤ(t)=" << ru << " == kᵤ(t-1)=" << ku_prev
                    << " → min=" << minU << ", max=" << maxU);
    }
    else
    {
        // =====================================================
        // MISMATCH: rᵤ(t) ≠ kᵤ(t−1)
        // Device is NOT using the assigned configuration
        // → Search toward more robust configs (higher EC)
        //
        // minᵤ = kᵤ(t−1)
        // maxᵤ = |K| (paper) → m_liteMaxConfigIndex (0-indexed)
        // =====================================================
        minU = ku_prev;                  // min = kᵤ(t-1)
        maxU = m_liteMaxConfigIndex;     // max = |K|
        
        NS_LOG_DEBUG("ADR-Lite [Mismatch]: rᵤ(t)=" << ru << " != kᵤ(t-1)=" << ku_prev
                    << " → min=" << minU << ", max=" << maxU);
    }
    
    // =====================================================
    // Binary Search: kᵤ(t) = I₍⌊(maxᵤ + minᵤ)/2⌋₎
    // =====================================================
    ku_new = (maxU + minU) / 2;  // Integer division = floor
    
    // Clamp to valid range [0, |K|-1]
    ku_new = std::max(0, std::min(ku_new, m_liteMaxConfigIndex));

    const LiteConfiguration& newCfg = m_liteConfigurations[ku_new];
    NS_LOG_DEBUG("ADR-Lite: kᵤ(t) = floor((" << maxU << " + " << minU << ")/2) = " << ku_new
                << " | SF" << (int)newCfg.sf
                << " TP=" << newCfg.txPowerDbm << "dBm"
                << " CF=" << (int)newCfg.channelFreq
                << " CR=" << (int)newCfg.codingRate
                << " EC=" << newCfg.energyIndex);

    state.currentConfigIndex = ku_new;
    *newConfigIndex = ku_new;
}

uint8_t
AdrComponent::GetTxPowerIndexLite(double txPowerDbm) const
{
    if (txPowerDbm < 2)
        txPowerDbm = 2;
    if (txPowerDbm > 14)
        txPowerDbm = 14;
    return static_cast<uint8_t>((14 - txPowerDbm) / 2);
}

uint32_t
AdrComponent::GetChannelFrequencyHz(uint8_t channelIndex) const
{
    // EU868 mandatory channels
    switch (channelIndex)
    {
    case 0:
        return 868100000; // 868.1 MHz
    case 1:
        return 868300000; // 868.3 MHz
    default:
        return 868500000; // Default to channel 0
    }
}

uint8_t
AdrComponent::GetChannelIndexFromFrequency(uint32_t frequencyHz) const
{
    // EU868 channels
    if (frequencyHz == 868100000) return 0;
    if (frequencyHz == 868300000) return 1;
    if (frequencyHz == 868500000) return 2;
    return 0; // Default to channel 0
}

} // namespace lorawan
} // namespace ns3
