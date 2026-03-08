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
                          MakeBooleanChecker());
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
            // Step 1: Algorithm 1 binary search for SF
            // =========================================================
            int newConfigIndex;
            AdrLiteImplementation(&newConfigIndex, status, state);
            state.currentConfigIndex = newConfigIndex;

            const LiteConfiguration& newCfg = m_liteConfigurations[newConfigIndex];
            uint8_t newSf = newCfg.sf;
            uint8_t newDr = SfToDr(newSf);

            // =========================================================
            // Step 2: TxPower optimization from SNR margin
            //
            // ADR-Lite advantage: no 20-packet history required.
            // Use the LAST received packet's SNR to compute optimal TP.
            // This reduces interference from the FIRST downlink,
            // unlike standard ADR which waits for 20 packets.
            // =========================================================
            uint8_t currentSf = status->GetFirstReceiveWindowSpreadingFactor();
            double currentTxPower = status->GetMac()->GetTransmissionPowerDbm();
            double newTxPower = currentTxPower;

            // Get SNR from the last received packet
            EndDeviceStatus::ReceivedPacketInfo lastPktInfo =
                status->GetLastReceivedPacketInfo();
            if (!lastPktInfo.gwList.empty())
            {
                double rxPower = GetReceivedPower(lastPktInfo.gwList);
                double snr = RxPowerToSNR(rxPower);
                double reqSnr = threshold[newDr];
                double margin = snr - reqSnr;

                // Use margin to reduce TxPower (same logic as standard ADR).
                // Each step = 3dB SNR margin = 2dBm TxPower reduction.
                int steps = std::floor(margin / 3);

                // Spend steps on TxPower reduction only
                // (SF is already handled by binary search)
                newTxPower = currentTxPower;
                while (steps > 0 && newTxPower > min_transmissionPower)
                {
                    newTxPower -= 2;
                    steps--;
                }
                while (steps < 0 && newTxPower < max_transmissionPower)
                {
                    newTxPower += 2;
                    steps++;
                }

                NS_LOG_DEBUG("ADR-Lite: SNR=" << snr << " reqSNR=" << reqSnr
                             << " margin=" << margin
                             << " TP: " << currentTxPower << "->" << newTxPower << "dBm");
            }

            if (!m_toggleTxPower)
            {
                newTxPower = currentTxPower;
            }

            // =========================================================
            // Step 3: Send downlink only if SF or TP actually changed
            // =========================================================
            bool sfChanged = (newSf != currentSf);
            bool tpChanged = (newTxPower != currentTxPower);

            if (sfChanged || tpChanged)
            {
                int channels[] = {0, 1, 2};
                std::list<int> enabledChannels(channels, channels + 3);

                NS_LOG_DEBUG("ADR-Lite: LinkAdrReq DR=" << (unsigned)newDr
                             << " SF" << (int)newSf
                             << " TP=" << newTxPower << "dBm"
                             << " (sfChg=" << sfChanged
                             << " tpChg=" << tpChanged << ")");

                status->m_reply.frameHeader.AddLinkAdrReq(
                    newDr,
                    GetTxPowerIndexLite(newTxPower),
                    enabledChannels,
                    1);
                status->m_reply.frameHeader.SetAsDownlink();
                status->m_reply.macHeader.SetMType(
                    LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
                status->m_reply.needsReply = true;
            }
            else
            {
                NS_LOG_DEBUG("ADR-Lite: No change needed for device "
                             << deviceAddress << " (SF" << (int)newSf
                             << " TP=" << newTxPower << "dBm)");
            }
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
    // ADR-Lite Configuration Space: I_k = {SF_k, TP_k}
    //
    // Binary search on SF only. TP is kept at device's current
    // value to avoid reducing power to unreachable levels.
    //
    // Only include SFs whose DR can carry the application payload
    // + MAC command overhead (LinkAdrAns = 2 bytes).
    // EU868 maxMACPayload: DR0-DR2 (SF12-SF10) = 59 bytes,
    //                      DR3 (SF9) = 123 bytes,
    //                      DR4-DR5 (SF8-SF7) = 230 bytes.
    // This ensures ADR-Lite never assigns a DR that would cause
    // packet overflow, matching standard ADR's implicit behavior.
    //
    // Sorted ascending by EC: lowest SF first → highest SF last
    // ===========================================================

    // EU868 max MACPayload per DR (index = DR number)
    const std::vector<uint32_t> maxPayloadPerDr = {59, 59, 59, 123, 230, 230, 230, 230};

    // Estimate worst-case frame header size with MAC command responses
    // Normal frame header: 8 bytes (DevAddr=4 + FCtrl=1 + FCnt=2 + FPort=1)
    // With LinkAdrAns:     10 bytes (+2 bytes for the MAC command)
    const uint32_t macOverhead = 10;

    const double refTp = 14.0;
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

        LiteConfiguration config;
        config.sf = sf;
        config.txPowerDbm = refTp;
        config.channelFreq = 0;
        config.codingRate = 1;
        config.energyIndex = CalculateEnergyIndex(sf, refTp, 1);
        m_liteConfigurations.push_back(config);
    }

    // Already sorted by SF ascending = EC ascending
    m_liteMinConfigIndex = 0;
    m_liteMaxConfigIndex = static_cast<int>(m_liteConfigurations.size()) - 1;

    NS_LOG_INFO("ADR-Lite: Initialized |K|=" << m_liteConfigurations.size()
                << " safe configurations");
    for (size_t i = 0; i < m_liteConfigurations.size(); ++i)
    {
        NS_LOG_INFO("ADR-Lite: I_" << i << ": SF" << (int)m_liteConfigurations[i].sf
                    << " (DR" << (int)SfToDr(m_liteConfigurations[i].sf)
                    << ") EC=" << m_liteConfigurations[i].energyIndex);
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
        newState.currentConfigIndex = 0;  // Placeholder; set to ru(0) on first uplink
        newState.initialized = false;
        newState.lastSentConfigIndex = -1;
        newState.awaitingAck = false;
        newState.lastAssignedSf = 7;
        newState.lastAssignedTxPower = 14;
        newState.lastAssignedCF = 0;
        newState.lastAssignedCR = 1;

        m_deviceLiteStates[deviceAddress] = newState;

        NS_LOG_DEBUG("ADR-Lite: New device " << deviceAddress
                     << " state created (will init ku(0) on first uplink)");
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

void
AdrComponent::AdrLiteImplementation(int* newConfigIndex, 
                                    Ptr<EndDeviceStatus> status,
                                    DeviceAdrLiteState& state)
{
    // =====================================================
    // Algorithm 1: ADR-Lite on NS
    //
    // Input:  ku(t-1) = state.currentConfigIndex
    //         ru(t)   = config index from device's last uplink
    // Output: ku(t)   = *newConfigIndex
    //
    // Initialization: ku(0) = ru(0)
    //   Start from the device's observed config so we don't
    //   push devices to high SFs unnecessarily.
    //
    // K = {I_0, I_1, ..., I_|K|-1} sorted ascending by EC
    //   I_0 = SF7  (lowest energy, least robust)
    //   I_{|K|-1} = most robust config in space
    // =====================================================
    NS_LOG_FUNCTION(this << status);

    // --- ru(t): config index observed from device's last uplink ---
    uint8_t rxSf = status->GetFirstReceiveWindowSpreadingFactor();
    int ru = FindConfigIndex(rxSf, status->GetMac()->GetTransmissionPowerDbm());

    // --- ku(t-1): previously assigned config index ---
    int ku_prev = state.currentConfigIndex;

    // =====================================================
    // Initialization: ku(0) = ru(0)
    // On first contact, set ku to the device's current config.
    // This avoids unnecessary SF changes for devices already
    // at optimal SF, while the binary search naturally adapts
    // if the environment changes (ru != ku on future packets).
    // =====================================================
    if (!state.initialized)
    {
        ku_prev = ru;
        state.currentConfigIndex = ru;
        state.initialized = true;
        NS_LOG_INFO("ADR-Lite [Init]: ku(0)=ru(0)=" << ru
                    << " (SF" << (int)rxSf << ")");
    }

    // =====================================================
    // Lines 12-17: Determine binary search bounds
    // =====================================================
    int min_u, max_u;

    if (ru == ku_prev)
    {
        // ru(t) == ku(t-1): device used assigned config
        // -> Search toward LOWER energy (more efficient)
        min_u = 0;
        max_u = ku_prev;
    }
    else
    {
        // ru(t) != ku(t-1): device did NOT use assigned config
        // -> Search toward HIGHER energy (more robust)
        min_u = ku_prev;
        max_u = m_liteMaxConfigIndex;
    }

    // =====================================================
    // Line 19: ku(t) = floor((max_u + min_u) / 2)
    // =====================================================
    int ku_new = (max_u + min_u) / 2;

    NS_LOG_DEBUG("ADR-Lite: ru=" << ru << " (SF" << (int)rxSf
                << ") ku(t-1)=" << ku_prev
                << " -> [" << min_u << "," << max_u
                << "] -> ku(t)=" << ku_new
                << " (SF" << (int)m_liteConfigurations[ku_new].sf << ")");

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

} // namespace lorawan
} // namespace ns3
