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
 * This component implements the ADR-Lite algorithm which uses binary search
 * to find optimal transmission parameters. Unlike traditional ADR that uses
 * SNR history of 20 packets, ADR-Lite only considers the last transmission
 * result (success/failure) to adjust parameters.
 * 
 * Key features:
 * - No packet history required (O(1) space complexity per device)
 * - Binary search for fast convergence
 * - Configurations sorted by energy consumption
 * - Adaptable to varying channel conditions
 */
class AdrLiteComponent : public NetworkControllerComponent
{
  public:
    /**
     * Structure representing a LoRaWAN transmission configuration.
     * Configurations are sorted by energy consumption (ToA).
     */
    struct Configuration
    {
        uint8_t sf;          //!< Spreading Factor (7-12)
        double txPowerDbm;   //!< Transmission power in dBm (2-14)
        double energyIndex;  //!< Relative energy consumption index
        
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
        int currentConfigIndex;    //!< k_u(t-1): Current assigned configuration index
        int lastReceivedConfigIndex; //!< r_u(t): Config index of last received packet
        bool initialized;          //!< Whether the device has been initialized
        uint8_t lastAssignedSf;    //!< Last assigned SF
        double lastAssignedTxPower; //!< Last assigned TxPower
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
     * Calculate Time on Air for a given SF (used for energy ordering).
     * 
     * @param sf Spreading factor (7-12)
     * @param payloadBytes Payload size in bytes
     * @return Time on Air in milliseconds
     */
    double CalculateToA(uint8_t sf, int payloadBytes) const;

    /**
     * Calculate energy consumption index for a configuration.
     * 
     * @param sf Spreading factor
     * @param txPowerDbm Transmission power in dBm
     * @return Energy consumption index
     */
    double CalculateEnergyIndex(uint8_t sf, double txPowerDbm) const;

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
    
    // LoRa parameters
    const int m_bandwidth = 125000;  //!< Bandwidth in Hz
    const int m_preambleSymbols = 8; //!< Number of preamble symbols
    const int m_payloadBytes = 20;   //!< Default payload size
    const bool m_headerEnabled = true; //!< Whether header is enabled
    const int m_codingRate = 1;      //!< Coding rate (1 = 4/5)

    bool m_toggleTxPower;  //!< Whether to adjust transmission power

    // SF-specific SNR thresholds for validation (dB)
    double m_snrThresholds[6] = {-20.0, -17.5, -15.0, -12.5, -10.0, -7.5};
};

} // namespace lorawan
} // namespace ns3

#endif // ADR_LITE_COMPONENT_H
