/*
 * Copyright (c) 2026
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
#include "end-device-lorawan-mac.h"

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
                          "Whether to adjust transmission power (TP_k)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&AdrLiteComponent::m_toggleTxPower),
                          MakeBooleanChecker())
            .AddAttribute("ChangeCodingRate",
                          "Whether to adjust coding rate (CR_k)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&AdrLiteComponent::m_toggleCodingRate),
                          MakeBooleanChecker())
            .AddAttribute("ChangeChannel",
                          "Whether to adjust channel frequency (CF_k)",
                          BooleanValue(true),
                          MakeBooleanAccessor(&AdrLiteComponent::m_toggleChannel),
                          MakeBooleanChecker());
    return tid;
}

AdrLiteComponent::AdrLiteComponent()
    : m_minConfigIndex(0),
      m_maxConfigIndex(0),
      m_toggleTxPower(true),
      m_toggleCodingRate(true),
      m_toggleChannel(true)
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
    
    /**
     * Create configuration space K = {I_1, I_2, ..., I_|K|}
     * where I_k = {SF_k, TP_k, CF_k, CR_k}
     * 
     * All 4 parameters are dynamically adjusted:
     *   - SF: 7-12 (6 values)
     *   - TP: 2, 4, 6, 8, 10, 12, 14 dBm (7 values)
     *   - CF: Channel index 0, 1, 2 (3 values)
     *   - CR: Coding rate 1=4/5, 2=4/6, 3=4/7, 4=4/8 (4 values)
     * 
     * Total configurations: 6 × 7 × 3 × 4 = 504
     * Configurations are sorted by Energy Consumption (EC) ascending
     */
    
    m_configurations.clear();
    
    // EU868 TxPower levels (dBm) - TP_k
    std::vector<double> txPowerLevels = {14, 12, 10, 8, 6, 4, 2};
    
    // Channel frequency indices - CF_k (EU868 mandatory channels)
    std::vector<uint8_t> channelIndices = {0, 1, 2};  // 868.1, 868.3, 868.5 MHz
    
    // Coding rates - CR_k
    std::vector<uint8_t> codingRates = {1, 2, 3, 4};  // 4/5, 4/6, 4/7, 4/8
    
    // Generate all configurations: I_k = {SF_k, TP_k, CF_k, CR_k}
    for (uint8_t sf = 7; sf <= 12; ++sf)
    {
        for (double txPower : txPowerLevels)
        {
            for (uint8_t cf : channelIndices)
            {
                for (uint8_t cr : codingRates)
                {
                    Configuration config;
                    config.sf = sf;              // SF_k
                    config.txPowerDbm = txPower; // TP_k
                    config.channelFreq = cf;     // CF_k
                    config.codingRate = cr;      // CR_k
                    config.energyIndex = CalculateEnergyIndex(sf, txPower, cr);
                    m_configurations.push_back(config);
                }
            }
        }
    }
    
    // Algorithm line 5: Sort K ascending according to EC (Energy Consumption)
    // Lower index = less energy = less robust (SF7, low power, low CR)
    // Higher index = more energy = more robust (SF12, high power, high CR)
    std::sort(m_configurations.begin(), m_configurations.end());
    
    m_minConfigIndex = 0;
    m_maxConfigIndex = static_cast<int>(m_configurations.size()) - 1;
    
    NS_LOG_INFO("ADR-Lite: Initialized |K|=" << m_configurations.size() << " configurations");
    NS_LOG_INFO("ADR-Lite: I_k = {SF_k, TP_k, CF_k, CR_k} - All parameters variable");
    NS_LOG_INFO("ADR-Lite: I_1 (min EC): SF" << (int)m_configurations[0].sf 
                << ", TP=" << m_configurations[0].txPowerDbm << " dBm"
                << ", CF=" << (int)m_configurations[0].channelFreq
                << ", CR=" << (int)m_configurations[0].codingRate);
    NS_LOG_INFO("ADR-Lite: I_|K| (max EC): SF" 
                << (int)m_configurations[m_maxConfigIndex].sf 
                << ", TP=" << m_configurations[m_maxConfigIndex].txPowerDbm << " dBm"
                << ", CF=" << (int)m_configurations[m_maxConfigIndex].channelFreq
                << ", CR=" << (int)m_configurations[m_maxConfigIndex].codingRate);
}

double
AdrLiteComponent::CalculateToA(uint8_t sf, int payloadBytes, uint8_t cr) const
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
    
    // CR_k = coding rate: 1=4/5, 2=4/6, 3=4/7, 4=4/8
    // Higher CR increases ToA but improves robustness
    int CR = cr;
    
    // Calculate payload symbols
    double numerator = 8.0 * payloadBytes - 4.0 * sf + 28.0 + 16.0 - 20.0 * H;
    double denominator = 4.0 * (sf - 2.0 * DE);
    
    int nPayload = 8 + std::max(static_cast<int>(std::ceil(numerator / denominator)) * (CR + 4), 0);
    
    double tPayload = nPayload * tSymbol;
    
    return (tPreamble + tPayload) * 1000.0; // Convert to milliseconds
}

double
AdrLiteComponent::CalculateEnergyIndex(uint8_t sf, double txPowerDbm, uint8_t cr) const
{
    // Energy = P_tx * ToA
    // EC(I_k) where I_k = {SF_k, TP_k, CF_k, CR_k}
    // We use a relative index where:
    // - ToA increases with SF (exponentially)
    // - ToA increases with CR (linearly)
    // - Power increases with TxPower (linearly in dBm, exponentially in mW)
    // Note: CF doesn't affect energy consumption directly
    
    double toA = CalculateToA(sf, m_payloadBytes, cr);
    
    // Convert TxPower from dBm to mW for energy calculation
    double txPowerMw = std::pow(10.0, txPowerDbm / 10.0);
    
    // Energy index (arbitrary units, used only for ordering)
    return toA * txPowerMw;
}

AdrLiteComponent::DeviceAdrState&
AdrLiteComponent::GetDeviceState(LoraDeviceAddress deviceAddress)
{
    /**
     * Algorithm line 2: Set u ∈ U to be the uth ED
     * 
     * In this implementation:
     *   - U = set of all End Devices (identified by LoraDeviceAddress)
     *   - u = deviceAddress (unique identifier for each ED)
     *   - m_deviceStates[deviceAddress] = state for the uth ED
     */
    
    auto it = m_deviceStates.find(deviceAddress);
    if (it == m_deviceStates.end())
    {
        // Algorithm line 8: Set k_u(0) = |K|
        // Initialize new device with most robust configuration (highest index)
        // In 0-based indexing: k_u(0) = m_maxConfigIndex = |K| - 1
        DeviceAdrState newState;
        newState.currentConfigIndex = m_maxConfigIndex;  // k_u(0) = |K| (0-based: |K|-1)
        newState.lastReceivedConfigIndex = -1;           // No packet received yet
        newState.initialized = true;
        
        // I_k = {SF_k, TP_k, CF_k, CR_k} for the initial configuration
        const Configuration& initConfig = m_configurations[m_maxConfigIndex];
        newState.lastAssignedSf = initConfig.sf;
        newState.lastAssignedTxPower = initConfig.txPowerDbm;
        newState.lastAssignedCF = initConfig.channelFreq;
        newState.lastAssignedCR = initConfig.codingRate;
        
        m_deviceStates[deviceAddress] = newState;
        
        NS_LOG_INFO("ADR-Lite: New device " << deviceAddress 
                    << " initialized with k_u(0)=" << m_maxConfigIndex
                    << " (SF" << (int)newState.lastAssignedSf 
                    << ", TP=" << newState.lastAssignedTxPower << " dBm"
                    << ", CF=" << (int)newState.lastAssignedCF
                    << ", CR=" << (int)newState.lastAssignedCR << ")");
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
    
    // Check CF and CR if they are being adjusted
    bool cfMatches = true;
    bool crMatches = true;
    
    if (m_toggleChannel)
    {
        // For CF, we check the channel index used
        // In this simplified model, we assume CF matches if state was set correctly
        cfMatches = true;  // CF is controlled by ChMask in LinkAdrReq
    }
    
    if (m_toggleCodingRate)
    {
        // Check if the device is using the assigned coding rate
        Ptr<EndDeviceLorawanMac> edMac = 
            DynamicCast<EndDeviceLorawanMac>(status->GetMac());
        if (edMac)
        {
            uint8_t receivedCr = edMac->GetCodingRate();
            crMatches = (receivedCr == assignedConfig.codingRate);
        }
    }
    
    NS_LOG_DEBUG("ADR-Lite: Received SF" << (int)receivedSf 
                 << " TP=" << receivedTxPower 
                 << " CR=" << (int)assignedConfig.codingRate
                 << " | Assigned SF" << (int)assignedConfig.sf 
                 << " TP=" << assignedConfig.txPowerDbm
                 << " CF=" << (int)assignedConfig.channelFreq
                 << " CR=" << (int)assignedConfig.codingRate
                 << " | Match: " << (sfMatches && txPowerMatches && cfMatches && crMatches));
    
    return sfMatches && 
           (txPowerMatches || !m_toggleTxPower) && 
           (cfMatches || !m_toggleChannel) && 
           (crMatches || !m_toggleCodingRate);
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
    int oldConfigIndex = state.currentConfigIndex;  // Save k_u(t-1) for logging
    bool parametersChanged = AdrLiteImplementation(&newConfigIndex, status);
    
    if (parametersChanged)
    {
        const Configuration& newConfig = m_configurations[newConfigIndex];
        
        // Update state: k_u(t) = {SF_k, TP_k, CF_k, CR_k} becomes k_u(t-1) for next iteration
        state.currentConfigIndex = newConfigIndex;
        state.lastAssignedSf = newConfig.sf;
        state.lastAssignedTxPower = newConfig.txPowerDbm;
        state.lastAssignedCF = newConfig.channelFreq;
        state.lastAssignedCR = newConfig.codingRate;
        
        // Create LinkAdrReq command with I_k = {SF_k, TP_k, CF_k, CR_k}
        uint8_t newDr = SfToDr(newConfig.sf);
        double newTxPowerDbm = m_toggleTxPower ? newConfig.txPowerDbm : currentTxPower;
        
        // CF_k: Channel frequency control
        // If m_toggleChannel is enabled, set only the assigned channel
        // Otherwise, enable all 3 mandatory EU868 channels
        std::list<int> enabledChannels;
        if (m_toggleChannel)
        {
            // Set ChMask to enable only the assigned channel CF_k
            enabledChannels.push_back(newConfig.channelFreq);
        }
        else
        {
            // Enable all mandatory channels (channel hopping)
            int channels[] = {0, 1, 2};
            enabledChannels = std::list<int>(channels, channels + sizeof(channels) / sizeof(int));
        }
        
        // CR_k: Coding rate control
        // Apply coding rate to the device if m_toggleCodingRate is enabled
        if (m_toggleCodingRate)
        {
            Ptr<EndDeviceLorawanMac> edMac = 
                DynamicCast<EndDeviceLorawanMac>(status->GetMac());
            if (edMac)
            {
                edMac->SetCodingRate(newConfig.codingRate);
                NS_LOG_DEBUG("ADR-Lite: Set CR_k=" << (int)newConfig.codingRate 
                             << " (4/" << (4 + newConfig.codingRate) << ")");
            }
        }
        
        // NbTrans (repetitions) is set to 1
        const int rep = 1;
        
        NS_LOG_INFO("ADR-Lite: Sending LinkAdrReq to device " << deviceAddress
                    << " | New: DR" << (int)newDr 
                    << " (SF" << (int)newConfig.sf << ")"
                    << ", TP=" << newTxPowerDbm << " dBm"
                    << ", CF=" << (int)newConfig.channelFreq
                    << ", CR=" << (int)newConfig.codingRate
                    << " | k_u: " << oldConfigIndex 
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
    
    /**
     * ADR-Lite Algorithm (Algorithm 1)
     * 
     * Notation mapping (1-based in paper → 0-based in code):
     *   - |K| = total configs (42) → m_maxConfigIndex + 1
     *   - ku(t-1) = previous assigned config → state.currentConfigIndex
     *   - ru(t) = config used in received packet
     *   - minu, maxu = binary search bounds
     *   - ku(t) = new assigned config
     * 
     * Algorithm:
     *   if ru(t) == ku(t-1):     (success: device used assigned config)
     *       minu = 1             → min_u = 0 (m_minConfigIndex)
     *       maxu = ku(t-1)       → max_u = k_prev
     *   else:                    (failure: device used different config)
     *       minu = ku(t-1)       → min_u = k_prev
     *       maxu = |K|           → max_u = m_maxConfigIndex (|K|-1 in 0-based)
     *   ku(t) = floor((maxu + minu) / 2)
     */
    
    // Get device address from MAC
    LoraDeviceAddress deviceAddress = status->m_endDeviceAddress;
    DeviceAdrState& state = GetDeviceState(deviceAddress);
    
    // k_u(t-1): previous assigned configuration index
    int k_prev = state.currentConfigIndex;
    
    int min_u, max_u;
    
    // Check if r_u(t) == k_u(t-1): received packet used assigned configuration
    bool success = ReceivedMatchesAssigned(status, state);
    
    if (success)
    {
        // Case: r_u(t) == k_u(t-1)
        // Device successfully received and applied assigned config
        // Search lower energy configurations (Algorithm line 12-14)
        min_u = m_minConfigIndex;  // minu = 1 (0-based: 0)
        max_u = k_prev;            // maxu = ku(t-1)
        
        NS_LOG_DEBUG("ADR-Lite: r_u(t)==k_u(t-1) SUCCESS - min_u=" 
                     << min_u << " max_u=" << max_u);
    }
    else
    {
        // Case: r_u(t) != k_u(t-1)
        // Device did not use assigned config (downlink lost or not applied)
        // Search higher energy/more robust configurations (Algorithm line 15-17)
        min_u = k_prev;            // minu = ku(t-1)
        max_u = m_maxConfigIndex;  // maxu = |K| (0-based: |K|-1)
        
        NS_LOG_DEBUG("ADR-Lite: r_u(t)!=k_u(t-1) FAILURE - min_u=" 
                     << min_u << " max_u=" << max_u);
    }
    
    // Algorithm line 19: k_u(t) = floor((max_u + min_u) / 2)
    // Integer division in C++ automatically floors
    int k_new = (max_u + min_u) / 2;
    
    // Ensure we stay within valid bounds [0, |K|-1]
    k_new = std::max(m_minConfigIndex, std::min(m_maxConfigIndex, k_new));
    
    NS_LOG_INFO("ADR-Lite: k_u(t) = floor((" << max_u << " + " << min_u 
                << ") / 2) = " << k_new 
                << " | k_u(t-1)=" << k_prev);
    
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
