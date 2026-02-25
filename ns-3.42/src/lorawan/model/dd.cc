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

    // Execute the Adaptive Data Rate (ADR) algorithm only if the request bit is set
    if (fHdr.GetAdr())
    {
        if (m_useAdrLite)
        {
            /////////////////////////////
            // ADR-Lite (binary search) //
            /////////////////////////////
            NS_LOG_DEBUG("ADR-Lite: New request from device " << fHdr.GetAddress());

            // Lazily build the configuration space on first use
            if (m_liteConfigurations.empty())
            {
                InitializeLiteConfigurationSpace();
            }

            LoraDeviceAddress deviceAddress = fHdr.GetAddress();
            DeviceAdrLiteState& state = GetDeviceLiteState(deviceAddress);

            uint8_t currentSf = status->GetFirstReceiveWindowSpreadingFactor();
            double currentTxPower = status->GetMac()->GetTransmissionPowerDbm();

            int newConfigIndex;
            int oldConfigIndex = state.currentConfigIndex;
            bool changed = AdrLiteImplementation(&newConfigIndex, status);

            if (changed)
            {
                const LiteConfiguration& newCfg = m_liteConfigurations[newConfigIndex];

                // Update device state for next iteration
                state.currentConfigIndex = newConfigIndex;
                state.lastAssignedSf = newCfg.sf;
                state.lastAssignedTxPower = newCfg.txPowerDbm;
                state.lastAssignedCF = newCfg.channelFreq;
                state.lastAssignedCR = newCfg.codingRate;

                uint8_t newDr = SfToDr(newCfg.sf);
                double newTxPowerDbm = m_toggleTxPower ? newCfg.txPowerDbm : currentTxPower;

                // CF_k: channel control
                std::list<int> enabledChannels;
                if (m_toggleChannel)
                {
                    enabledChannels.push_back(newCfg.channelFreq);
                }
                else
                {
                    int channels[] = {0, 1, 2};
                    enabledChannels = std::list<int>(channels, channels + 3);
                }

                // CR_k: coding rate control
                if (m_toggleCodingRate)
                {
                    Ptr<EndDeviceLorawanMac> edMac =
                        DynamicCast<EndDeviceLorawanMac>(status->GetMac());
                    if (edMac)
                    {
                        edMac->SetCodingRate(newCfg.codingRate);
                    }
                }

                const int rep = 1;

                NS_LOG_DEBUG("ADR-Lite: LinkAdrReq DR=" << (unsigned)newDr
                             << " TP=" << newTxPowerDbm << "dBm"
                             << " CF=" << (int)newCfg.channelFreq
                             << " CR=" << (int)newCfg.codingRate
                             << " k_u: " << oldConfigIndex << "->" << newConfigIndex);

                status->m_reply.frameHeader.AddLinkAdrReq(newDr,
                                                          GetTxPowerIndexLite(newTxPowerDbm),
                                                          enabledChannels,
                                                          rep);
                status->m_reply.frameHeader.SetAsDownlink();
                status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
                status->m_reply.needsReply = true;
            }
            else
            {
                NS_LOG_DEBUG("ADR-Lite: No change for device " << deviceAddress);
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

    if (m_useAdrLite)
    {
        // ADR-Lite: move towards more robust config on failed reply
        LoraDeviceAddress deviceAddress = status->m_endDeviceAddress;
        auto it = m_deviceLiteStates.find(deviceAddress);
        if (it != m_deviceLiteStates.end())
        {
            DeviceAdrLiteState& state = it->second;
            int newIndex = (state.currentConfigIndex + m_liteMaxConfigIndex) / 2;
            newIndex = std::min(newIndex + 1, m_liteMaxConfigIndex);

            NS_LOG_WARN("ADR-Lite: Reply failed for device " << deviceAddress
                        << " | config " << state.currentConfigIndex << " -> " << newIndex);

            state.currentConfigIndex = newIndex;
            state.lastAssignedSf = m_liteConfigurations[newIndex].sf;
            state.lastAssignedTxPower = m_liteConfigurations[newIndex].txPowerDbm;
            state.lastAssignedCF = m_liteConfigurations[newIndex].channelFreq;
            state.lastAssignedCR = m_liteConfigurations[newIndex].codingRate;
        }
    }
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

    // EU868 TxPower levels (dBm)
    std::vector<double> txPowerLevels = {14, 12, 10, 8, 6, 4, 2};
    // Channel frequency indices (EU868 mandatory)
    std::vector<uint8_t> channelIndices = {0, 1, 2};
    // Coding rates: 1=4/5, 2=4/6, 3=4/7, 4=4/8
    std::vector<uint8_t> codingRates = {1, 2, 3, 4};

    for (uint8_t sf = 7; sf <= 12; ++sf)
    {
        for (double txPower : txPowerLevels)
        {
            for (uint8_t cf : channelIndices)
            {
                for (uint8_t cr : codingRates)
                {
                    LiteConfiguration config;
                    config.sf = sf;
                    config.txPowerDbm = txPower;
                    config.channelFreq = cf;
                    config.codingRate = cr;
                    config.energyIndex = CalculateEnergyIndex(sf, txPower, cr);
                    m_liteConfigurations.push_back(config);
                }
            }
        }
    }

    // Sort ascending by energy consumption
    std::sort(m_liteConfigurations.begin(), m_liteConfigurations.end());

    m_liteMinConfigIndex = 0;
    m_liteMaxConfigIndex = static_cast<int>(m_liteConfigurations.size()) - 1;

    NS_LOG_INFO("ADR-Lite: Initialized |K|=" << m_liteConfigurations.size()
                << " configurations (6 SF x 7 TP x 3 CF x 4 CR)");
    NS_LOG_INFO("ADR-Lite: I_1 (min EC): SF" << (int)m_liteConfigurations[0].sf
                << " TP=" << m_liteConfigurations[0].txPowerDbm << "dBm"
                << " CF=" << (int)m_liteConfigurations[0].channelFreq
                << " CR=" << (int)m_liteConfigurations[0].codingRate);
    NS_LOG_INFO("ADR-Lite: I_|K| (max EC): SF"
                << (int)m_liteConfigurations[m_liteMaxConfigIndex].sf
                << " TP=" << m_liteConfigurations[m_liteMaxConfigIndex].txPowerDbm << "dBm"
                << " CF=" << (int)m_liteConfigurations[m_liteMaxConfigIndex].channelFreq
                << " CR=" << (int)m_liteConfigurations[m_liteMaxConfigIndex].codingRate);
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
        // k_u(0) = |K|-1 (most robust config, 0-based indexing)
        DeviceAdrLiteState newState;
        newState.currentConfigIndex = m_liteMaxConfigIndex;
        newState.initialized = true;

        const LiteConfiguration& initCfg = m_liteConfigurations[m_liteMaxConfigIndex];
        newState.lastAssignedSf = initCfg.sf;
        newState.lastAssignedTxPower = initCfg.txPowerDbm;
        newState.lastAssignedCF = initCfg.channelFreq;
        newState.lastAssignedCR = initCfg.codingRate;

        m_deviceLiteStates[deviceAddress] = newState;

        NS_LOG_INFO("ADR-Lite: New device " << deviceAddress
                    << " k_u(0)=" << m_liteMaxConfigIndex
                    << " SF" << (int)newState.lastAssignedSf
                    << " TP=" << newState.lastAssignedTxPower << "dBm"
                    << " CF=" << (int)newState.lastAssignedCF
                    << " CR=" << (int)newState.lastAssignedCR);
    }
    return m_deviceLiteStates[deviceAddress];
}

bool
AdrComponent::ReceivedMatchesAssigned(Ptr<EndDeviceStatus> status,
                                      const DeviceAdrLiteState& state) const
{
    uint8_t receivedSf = status->GetFirstReceiveWindowSpreadingFactor();
    double receivedTxPower = status->GetMac()->GetTransmissionPowerDbm();

    const LiteConfiguration& assigned = m_liteConfigurations[state.currentConfigIndex];

    bool sfMatch = (receivedSf == assigned.sf);
    bool tpMatch = (std::abs(receivedTxPower - assigned.txPowerDbm) < 0.1);

    bool cfMatch = true;
    bool crMatch = true;

    if (m_toggleCodingRate)
    {
        Ptr<EndDeviceLorawanMac> edMac =
            DynamicCast<EndDeviceLorawanMac>(status->GetMac());
        if (edMac)
        {
            crMatch = (edMac->GetCodingRate() == assigned.codingRate);
        }
    }

    NS_LOG_DEBUG("ADR-Lite: Rx SF" << (int)receivedSf << " TP=" << receivedTxPower
                 << " | Assigned SF" << (int)assigned.sf << " TP=" << assigned.txPowerDbm
                 << " CF=" << (int)assigned.channelFreq << " CR=" << (int)assigned.codingRate
                 << " | match=" << (sfMatch && tpMatch && cfMatch && crMatch));

    return sfMatch &&
           (tpMatch || !m_toggleTxPower) &&
           (cfMatch || !m_toggleChannel) &&
           (crMatch || !m_toggleCodingRate);
}

bool
AdrComponent::AdrLiteImplementation(int* newConfigIndex, Ptr<EndDeviceStatus> status)
{
    NS_LOG_FUNCTION(this << status);

    LoraDeviceAddress deviceAddress = status->m_endDeviceAddress;
    DeviceAdrLiteState& state = GetDeviceLiteState(deviceAddress);

    int k_prev = state.currentConfigIndex;
    int min_u, max_u;

    bool success = ReceivedMatchesAssigned(status, state);

    if (success)
    {
        // r_u(t) == k_u(t-1): search lower energy
        min_u = m_liteMinConfigIndex;
        max_u = k_prev;
        NS_LOG_DEBUG("ADR-Lite: SUCCESS min_u=" << min_u << " max_u=" << max_u);
    }
    else
    {
        // r_u(t) != k_u(t-1): search higher energy / more robust
        min_u = k_prev;
        max_u = m_liteMaxConfigIndex;
        NS_LOG_DEBUG("ADR-Lite: FAILURE min_u=" << min_u << " max_u=" << max_u);
    }

    // k_u(t) = floor((max_u + min_u) / 2)
    int k_new = (max_u + min_u) / 2;
    k_new = std::max(m_liteMinConfigIndex, std::min(m_liteMaxConfigIndex, k_new));

    NS_LOG_INFO("ADR-Lite: k_u(t) = floor((" << max_u << "+" << min_u
                << ")/2) = " << k_new << "  k_u(t-1)=" << k_prev);

    *newConfigIndex = k_new;

    const LiteConfiguration& oldCfg = m_liteConfigurations[k_prev];
    const LiteConfiguration& newCfg = m_liteConfigurations[k_new];

    bool changed = (newCfg.sf != oldCfg.sf);
    if (m_toggleTxPower)
    {
        changed = changed || (std::abs(newCfg.txPowerDbm - oldCfg.txPowerDbm) > 0.1);
    }
    if (m_toggleCodingRate)
    {
        changed = changed || (newCfg.codingRate != oldCfg.codingRate);
    }
    if (m_toggleChannel)
    {
        changed = changed || (newCfg.channelFreq != oldCfg.channelFreq);
    }

    return changed;
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
