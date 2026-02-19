/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * ADR-Lite: A low-complexity Adaptive Data Rate algorithm for LoRaWAN
 * 
 * Implementation of the ADR-Lite algorithm using binary search for
 * optimal transmission parameter selection.
 */

#include "adr-lite-component.h"
#include "lora-frame-header.h"
#include "lorawan-mac-header.h"
#include "end-device-status.h"

#include "ns3/log.h"

#include <algorithm>
#include <cmath>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("AdrLiteComponent");

NS_OBJECT_ENSURE_REGISTERED(AdrLiteComponent);

TypeId
AdrLiteComponent::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AdrLiteComponent")
            .SetGroupName("lorawan")
            .AddConstructor<AdrLiteComponent>()
            .SetParent<NetworkControllerComponent>()
            .AddAttribute("ChangeTransmissionPower",
                          "Whether to adjust transmission power or only SF",
                          BooleanValue(true),
                          MakeBooleanAccessor(&AdrLiteComponent::m_toggleTxPower),
                          MakeBooleanChecker());
    return tid;
}

AdrLiteComponent::AdrLiteComponent()
    : m_minConfigIndex(0),
      m_maxConfigIndex(0),
      m_toggleTxPower(true)
{
    NS_LOG_FUNCTION(this);
    InitializeConfigurationSpace();
}

AdrLiteComponent::~AdrLiteComponent()
{
    NS_LOG_FUNCTION(this);
}

void
AdrLiteComponent::InitializeConfigurationSpace()
{
    NS_LOG_FUNCTION(this);
    
    // Create all possible configurations (SF x TxPower)
    // SF: 7-12 (6 values)
    // TxPower: 2, 4, 6, 8, 10, 12, 14 dBm (7 values)
    // Total: 42 configurations
    
    m_configurations.clear();
    
    // EU868 TxPower levels (dBm)
    std::vector<double> txPowerLevels = {14, 12, 10, 8, 6, 4, 2};
    
    for (uint8_t sf = 7; sf <= 12; ++sf)
    {
        for (double txPower : txPowerLevels)
        {
            Configuration config;
            config.sf = sf;
            config.txPowerDbm = txPower;
            config.energyIndex = CalculateEnergyIndex(sf, txPower);
            m_configurations.push_back(config);
        }
    }
    
    // Sort configurations by energy consumption (ascending)
    // Lower index = less energy = less robust (SF7, low power)
    // Higher index = more energy = more robust (SF12, high power)
    std::sort(m_configurations.begin(), m_configurations.end());
    
    m_minConfigIndex = 0;
    m_maxConfigIndex = static_cast<int>(m_configurations.size()) - 1;
    
    NS_LOG_INFO("ADR-Lite: Initialized " << m_configurations.size() << " configurations");
    NS_LOG_INFO("ADR-Lite: Config 0 (min energy): SF" << (int)m_configurations[0].sf 
                << ", TxP=" << m_configurations[0].txPowerDbm << " dBm");
    NS_LOG_INFO("ADR-Lite: Config " << m_maxConfigIndex << " (max energy): SF" 
                << (int)m_configurations[m_maxConfigIndex].sf 
                << ", TxP=" << m_configurations[m_maxConfigIndex].txPowerDbm << " dBm");
}

double
AdrLiteComponent::CalculateToA(uint8_t sf, int payloadBytes) const
{
    // Calculate Time on Air based on LoRa PHY formulas
    // T_symbol = 2^SF / BW
    double tSymbol = std::pow(2.0, sf) / m_bandwidth;
    
    // T_preamble = (4.25 + N_preamble) * T_symbol
    double tPreamble = (4.25 + m_preambleSymbols) * tSymbol;
    
    // Payload symbols calculation
    // H = 0 if header enabled, 1 if disabled
    int H = m_headerEnabled ? 0 : 1;
    
    // DE = 1 for SF11/SF12 with 125kHz bandwidth (low data rate optimization)
    int DE = (sf >= 11) ? 1 : 0;
    
    // CR = coding rate (1 = 4/5)
    int CR = m_codingRate;
    
    // Calculate payload symbols
    double numerator = 8.0 * payloadBytes - 4.0 * sf + 28.0 + 16.0 - 20.0 * H;
    double denominator = 4.0 * (sf - 2.0 * DE);
    
    int nPayload = 8 + std::max(static_cast<int>(std::ceil(numerator / denominator)) * (CR + 4), 0);
    
    double tPayload = nPayload * tSymbol;
    
    return (tPreamble + tPayload) * 1000.0; // Convert to milliseconds
}

double
AdrLiteComponent::CalculateEnergyIndex(uint8_t sf, double txPowerDbm) const
{
    // Energy = P_tx * ToA
    // We use a relative index where:
    // - ToA increases with SF (exponentially)
    // - Power increases with TxPower (linearly in dBm, exponentially in mW)
    
    double toA = CalculateToA(sf, m_payloadBytes);
    
    // Convert TxPower from dBm to mW for energy calculation
    double txPowerMw = std::pow(10.0, txPowerDbm / 10.0);
    
    // Energy index (arbitrary units, used only for ordering)
    return toA * txPowerMw;
}

AdrLiteComponent::DeviceAdrState&
AdrLiteComponent::GetDeviceState(LoraDeviceAddress deviceAddress)
{
    auto it = m_deviceStates.find(deviceAddress);
    if (it == m_deviceStates.end())
    {
        // Initialize new device state with most robust configuration
        DeviceAdrState newState;
        newState.currentConfigIndex = m_maxConfigIndex;  // k_u(0) = |K|
        newState.lastReceivedConfigIndex = -1;           // No packet received yet
        newState.initialized = true;
        newState.lastAssignedSf = m_configurations[m_maxConfigIndex].sf;
        newState.lastAssignedTxPower = m_configurations[m_maxConfigIndex].txPowerDbm;
        
        m_deviceStates[deviceAddress] = newState;
        
        NS_LOG_INFO("ADR-Lite: New device " << deviceAddress 
                    << " initialized with config " << m_maxConfigIndex
                    << " (SF" << (int)newState.lastAssignedSf 
                    << ", TxP=" << newState.lastAssignedTxPower << " dBm)");
    }
    return m_deviceStates[deviceAddress];
}

uint8_t
AdrLiteComponent::SfToDr(uint8_t sf) const
{
    // EU868 mapping: SF12=DR0, SF11=DR1, ..., SF7=DR5
    switch (sf)
    {
    case 12: return 0;
    case 11: return 1;
    case 10: return 2;
    case 9:  return 3;
    case 8:  return 4;
    case 7:
    default: return 5;
    }
}

uint8_t
AdrLiteComponent::GetTxPowerIndex(double txPowerDbm) const
{
    // EU868 TxPower index: 0=14dBm, 1=12dBm, ..., 7=0dBm
    // TxPowerIndex = (14 - txPowerDbm) / 2
    if (txPowerDbm < 2) txPowerDbm = 2;
    if (txPowerDbm > 14) txPowerDbm = 14;
    return static_cast<uint8_t>((14 - txPowerDbm) / 2);
}

bool
AdrLiteComponent::ReceivedMatchesAssigned(Ptr<EndDeviceStatus> status, 
                                           const DeviceAdrState& state) const
{
    // Get the SF used by the received packet
    uint8_t receivedSf = status->GetFirstReceiveWindowSpreadingFactor();
    
    // Get the TxPower used by the device
    double receivedTxPower = status->GetMac()->GetTransmissionPowerDbm();
    
    // Check if they match the assigned configuration
    const Configuration& assignedConfig = m_configurations[state.currentConfigIndex];
    
    bool sfMatches = (receivedSf == assignedConfig.sf);
    bool txPowerMatches = (std::abs(receivedTxPower - assignedConfig.txPowerDbm) < 0.1);
    
    NS_LOG_DEBUG("ADR-Lite: Received SF" << (int)receivedSf 
                 << " TxP=" << receivedTxPower 
                 << " | Assigned SF" << (int)assignedConfig.sf 
                 << " TxP=" << assignedConfig.txPowerDbm
                 << " | Match: " << (sfMatches && txPowerMatches));
    
    return sfMatches && (txPowerMatches || !m_toggleTxPower);
}

void
AdrLiteComponent::OnReceivedPacket(Ptr<const Packet> packet,
                                    Ptr<EndDeviceStatus> status,
                                    Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this << packet << status << networkStatus);
    
    // We record the reception but actual ADR decision is made in BeforeSendingReply
    // This allows us to consider all gateway receptions
}

void
AdrLiteComponent::BeforeSendingReply(Ptr<EndDeviceStatus> status, 
                                      Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this << status << networkStatus);
    
    // Get the last received packet
    Ptr<Packet> myPacket = status->GetLastPacketReceivedFromDevice()->Copy();
    LorawanMacHeader mHdr;
    LoraFrameHeader fHdr;
    fHdr.SetAsUplink();
    myPacket->RemoveHeader(mHdr);
    myPacket->RemoveHeader(fHdr);
    
    // Only execute ADR if the ADR bit is set
    if (!fHdr.GetAdr())
    {
        NS_LOG_DEBUG("ADR-Lite: ADR bit not set, skipping");
        return;
    }
    
    // Get device address and state
    LoraDeviceAddress deviceAddress = fHdr.GetAddress();
    DeviceAdrState& state = GetDeviceState(deviceAddress);
    
    // Get current device parameters
    uint8_t currentSf = status->GetFirstReceiveWindowSpreadingFactor();
    double currentTxPower = status->GetMac()->GetTransmissionPowerDbm();
    
    NS_LOG_INFO("ADR-Lite: Processing device " << deviceAddress
                << " | Current: SF" << (int)currentSf 
                << ", TxP=" << currentTxPower << " dBm"
                << " | ConfigIndex=" << state.currentConfigIndex);
    
    // Execute ADR-Lite algorithm
    int newConfigIndex;
    bool parametersChanged = AdrLiteImplementation(&newConfigIndex, status);
    
    if (parametersChanged)
    {
        const Configuration& newConfig = m_configurations[newConfigIndex];
        
        // Update state
        state.currentConfigIndex = newConfigIndex;
        state.lastAssignedSf = newConfig.sf;
        state.lastAssignedTxPower = newConfig.txPowerDbm;
        
        // Create LinkAdrReq command
        uint8_t newDr = SfToDr(newConfig.sf);
        double newTxPowerDbm = m_toggleTxPower ? newConfig.txPowerDbm : currentTxPower;
        
        // Mandatory channels (EU868)
        int channels[] = {0, 1, 2};
        std::list<int> enabledChannels(channels, channels + sizeof(channels) / sizeof(int));
        
        const int rep = 1;  // NbTrans
        
        NS_LOG_INFO("ADR-Lite: Sending LinkAdrReq to device " << deviceAddress
                    << " | New: DR" << (int)newDr 
                    << " (SF" << (int)newConfig.sf << ")"
                    << ", TxP=" << newTxPowerDbm << " dBm"
                    << " | ConfigIndex: " << state.currentConfigIndex - 1 
                    << " -> " << newConfigIndex);
        
        status->m_reply.frameHeader.AddLinkAdrReq(newDr,
                                                   GetTxPowerIndex(newTxPowerDbm),
                                                   enabledChannels,
                                                   rep);
        status->m_reply.frameHeader.SetAsDownlink();
        status->m_reply.macHeader.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN);
        status->m_reply.needsReply = true;
    }
    else
    {
        NS_LOG_DEBUG("ADR-Lite: No parameter change needed for device " << deviceAddress);
    }
}

bool
AdrLiteComponent::AdrLiteImplementation(int* newConfigIndex, Ptr<EndDeviceStatus> status)
{
    NS_LOG_FUNCTION(this << status);
    
    // Get device address from MAC
    LoraDeviceAddress deviceAddress = status->m_endDeviceAddress;
    DeviceAdrState& state = GetDeviceState(deviceAddress);
    
    int k_prev = state.currentConfigIndex;  // k_u(t-1)
    // Note: |K| = m_maxConfigIndex + 1 (total number of configurations)
    
    int min_u, max_u;
    
    // Check if received packet matches assigned configuration
    bool success = ReceivedMatchesAssigned(status, state);
    
    if (success)
    {
        // Success case: r_u(t) == k_u(t-1)
        // Try lower energy configuration (binary search left)
        min_u = m_minConfigIndex;
        max_u = k_prev;
        
        NS_LOG_DEBUG("ADR-Lite: SUCCESS - searching lower energy configs ["
                     << min_u << ", " << max_u << "]");
    }
    else
    {
        // Failure case: received config doesn't match
        // Try higher energy/more robust configuration (binary search right)
        min_u = k_prev;
        max_u = m_maxConfigIndex;
        
        NS_LOG_DEBUG("ADR-Lite: MISMATCH - searching higher energy configs ["
                     << min_u << ", " << max_u << "]");
    }
    
    // Binary search: k_u(t) = floor((max_u + min_u) / 2)
    int k_new = (max_u + min_u) / 2;
    
    // Ensure we stay within bounds
    k_new = std::max(m_minConfigIndex, std::min(m_maxConfigIndex, k_new));
    
    NS_LOG_INFO("ADR-Lite: Binary search - k_prev=" << k_prev 
                << " | min=" << min_u << " max=" << max_u
                << " | k_new=" << k_new);
    
    *newConfigIndex = k_new;
    
    // Check if configuration actually changed
    const Configuration& oldConfig = m_configurations[k_prev];
    const Configuration& newConfig = m_configurations[k_new];
    
    bool changed = (newConfig.sf != oldConfig.sf);
    if (m_toggleTxPower)
    {
        changed = changed || (std::abs(newConfig.txPowerDbm - oldConfig.txPowerDbm) > 0.1);
    }
    
    return changed;
}

void
AdrLiteComponent::OnFailedReply(Ptr<EndDeviceStatus> status, 
                                 Ptr<NetworkStatus> networkStatus)
{
    NS_LOG_FUNCTION(this << status << networkStatus);
    
    // When a reply fails, we should move towards more robust configuration
    // This will be handled on the next received packet
    LoraDeviceAddress deviceAddress = status->m_endDeviceAddress;
    
    auto it = m_deviceStates.find(deviceAddress);
    if (it != m_deviceStates.end())
    {
        DeviceAdrState& state = it->second;
        
        // Move towards more robust configuration
        int newIndex = (state.currentConfigIndex + m_maxConfigIndex) / 2;
        newIndex = std::min(newIndex + 1, m_maxConfigIndex);
        
        NS_LOG_WARN("ADR-Lite: Reply failed for device " << deviceAddress
                    << " | Moving to more robust config: " 
                    << state.currentConfigIndex << " -> " << newIndex);
        
        state.currentConfigIndex = newIndex;
        state.lastAssignedSf = m_configurations[newIndex].sf;
        state.lastAssignedTxPower = m_configurations[newIndex].txPowerDbm;
    }
}

} // namespace lorawan
} // namespace ns3
