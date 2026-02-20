/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * ADR-Lite: A low-complexity Adaptive Data Rate algorithm for LoRaWAN
 * 
 * This algorithm uses a binary search approach to find optimal transmission
 * parameters without maintaining packet history. It configures SF and TxPower
 * based on the success/failure of the previous transmission.
 * 
 * Reference: ADR-Lite algorithm for LoRaWAN networks with varying channel conditions
 */

#ifndef ADR_LITE_COMPONENT_H
#define ADR_LITE_COMPONENT_H

#include "network-controller-components.h"
#include "network-status.h"

#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/packet.h"

#include <map>
#include <vector>

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * ADR-Lite: Low-complexity Adaptive Data Rate algorithm
 * 
 * Implementation of Algorithm 1: ADR-Lite on NS
 * 
 * Input:  k_u(t-1) = previous assigned config index
 *         r_u(t)   = config index used in last received packet
 * Output: k_u(t)   = new assigned config index
 * 
 * Algorithm:
 *   Initialization:
 *     - K = {I_1, I_2, ..., I_|K|} sorted ascending by Energy Consumption (EC)
 *     - I_k = {SF_k, TP_k, CF_k, CR_k}
 *     - k_u(0) = |K| (start with most robust config)
 * 
 *   For each received packet at iteration t:
 *     if r_u(t) == k_u(t-1):    // success: device used assigned config
 *         min_u = 1
 *         max_u = k_u(t-1)
 *     else:                      // failure: device used different config
 *         min_u = k_u(t-1)
 *         max_u = |K|
 *     k_u(t) = floor((max_u + min_u) / 2)    // binary search
 * 
 * Note: Implementation uses 0-based indexing (indices 0 to |K|-1)
 */
class AdrLiteComponent : public NetworkControllerComponent
{
  public:
    /**
     * Structure representing a LoRaWAN transmission configuration I_k.
     * 
     * Full configuration: I_k = {SF_k, TP_k, CF_k, CR_k}
     * All 4 parameters are dynamically adjusted:
     *   - SF_k: Spreading Factor (7-12) - 6 values
     *   - TP_k: Transmission Power (2-14 dBm) - 7 values
     *   - CF_k: Channel Frequency index (0, 1, 2) - 3 values
     *   - CR_k: Coding Rate (1=4/5, 2=4/6, 3=4/7, 4=4/8) - 4 values
     * 
     * Total configurations: 6 × 7 × 3 × 4 = 504 configurations
     * Configurations are sorted ascending by Energy Consumption (EC).
     */
    struct Configuration
    {
        uint8_t sf;          //!< SF_k: Spreading Factor (7-12)
        double txPowerDbm;   //!< TP_k: Transmission power in dBm (2-14)
        uint8_t channelFreq; //!< CF_k: Channel frequency index (0, 1, 2 = 868.1, 868.3, 868.5 MHz)
        uint8_t codingRate;  //!< CR_k: Coding rate (1=4/5, 2=4/6, 3=4/7, 4=4/8)
        double energyIndex;  //!< EC_k: Relative energy consumption index
        
        bool operator<(const Configuration& other) const
        {
            return energyIndex < other.energyIndex;
        }
    };

    /**
     * Structure to track device state for ADR-Lite algorithm.
     */
    struct DeviceAdrState
    {
        int currentConfigIndex;      //!< k_u(t-1): Current assigned configuration index
        int lastReceivedConfigIndex; //!< r_u(t): Config index of last received packet
        bool initialized;            //!< Whether the device has been initialized
        uint8_t lastAssignedSf;      //!< Last assigned SF_k
        double lastAssignedTxPower;  //!< Last assigned TP_k
        uint8_t lastAssignedCF;      //!< Last assigned CF_k (channel index)
        uint8_t lastAssignedCR;      //!< Last assigned CR_k (coding rate)
    };

    /**
     *  Register this type.
     *  @return The object TypeId.
     */
    static TypeId GetTypeId();

    AdrLiteComponent();           //!< Default constructor
    ~AdrLiteComponent() override; //!< Destructor

    void OnReceivedPacket(Ptr<const Packet> packet,
                          Ptr<EndDeviceStatus> status,
                          Ptr<NetworkStatus> networkStatus) override;

    void BeforeSendingReply(Ptr<EndDeviceStatus> status, 
                            Ptr<NetworkStatus> networkStatus) override;

    void OnFailedReply(Ptr<EndDeviceStatus> status, 
                       Ptr<NetworkStatus> networkStatus) override;

  private:
    /**
     * Initialize the configuration space K.
     * Configurations are sorted by energy consumption (based on ToA).
     */
    void InitializeConfigurationSpace();

    /**
     * Calculate Time on Air for a given SF and CR (used for energy ordering).
     * 
     * @param sf Spreading factor (7-12)
     * @param payloadBytes Payload size in bytes
     * @param cr Coding rate (1=4/5, 2=4/6, 3=4/7, 4=4/8)
     * @return Time on Air in milliseconds
     */
    double CalculateToA(uint8_t sf, int payloadBytes, uint8_t cr) const;

    /**
     * Calculate energy consumption index EC(I_k) for a configuration.
     * 
     * @param sf Spreading factor SF_k
     * @param txPowerDbm Transmission power TP_k in dBm
     * @param cr Coding rate CR_k (1-4)
     * @return Energy consumption index
     */
    double CalculateEnergyIndex(uint8_t sf, double txPowerDbm, uint8_t cr) const;

    /**
     * Get or create device ADR state.
     * 
     * @param deviceAddress The device address
     * @return Reference to the device's ADR state
     */
    DeviceAdrState& GetDeviceState(LoraDeviceAddress deviceAddress);

    /**
     * ADR-Lite binary search implementation.
     * 
     * @param newConfigIndex [out] New configuration index
     * @param status End device status
     * @return true if parameters changed
     */
    bool AdrLiteImplementation(int* newConfigIndex, Ptr<EndDeviceStatus> status);

    /**
     * Convert SF to data rate (EU868).
     * 
     * @param sf Spreading factor (7-12)
     * @return Data rate (0-5)
     */
    uint8_t SfToDr(uint8_t sf) const;

    /**
     * Convert TxPower in dBm to TxPower index.
     * 
     * @param txPowerDbm Power in dBm
     * @return TxPower index (0-7)
     */
    uint8_t GetTxPowerIndex(double txPowerDbm) const;

    /**
     * Check if received packet matches assigned configuration.
     * 
     * @param status End device status
     * @param state Device ADR state
     * @return true if received config matches assigned config
     */
    bool ReceivedMatchesAssigned(Ptr<EndDeviceStatus> status, 
                                  const DeviceAdrState& state) const;

    // Configuration space
    std::vector<Configuration> m_configurations;  //!< K: Set of configurations sorted by energy

    // Device states
    std::map<LoraDeviceAddress, DeviceAdrState> m_deviceStates;  //!< State per device

    // Algorithm parameters
    int m_minConfigIndex;      //!< Minimum configuration index (lowest energy)
    int m_maxConfigIndex;      //!< Maximum configuration index (highest energy/most robust)
    
    // LoRa PHY parameters for ToA calculation
    const int m_bandwidth = 125000;    //!< Bandwidth in Hz (BW)
    const int m_preambleSymbols = 8;   //!< Number of preamble symbols
    const int m_payloadBytes = 20;     //!< Default payload size for ToA calculation
    const bool m_headerEnabled = true; //!< Whether explicit header is enabled

    bool m_toggleTxPower;  //!< Whether to adjust transmission power
    bool m_toggleCodingRate;  //!< Whether to adjust coding rate (CR)
    bool m_toggleChannel;     //!< Whether to adjust channel frequency (CF)

    // SF-specific SNR thresholds for validation (dB)
    double m_snrThresholds[6] = {-20.0, -17.5, -15.0, -12.5, -10.0, -7.5};
};

} // namespace lorawan
} // namespace ns3

#endif // ADR_LITE_COMPONENT_H
