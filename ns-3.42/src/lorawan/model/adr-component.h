/*
 * Copyright (c) 2017 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Matteo Perin <matteo.perin.2@studenti.unipd.2>
 *
 * Extended with ADR-Lite: A low-complexity Adaptive Data Rate algorithm
 * using binary search for optimal transmission parameter selection.
 */

#ifndef ADR_COMPONENT_H
#define ADR_COMPONENT_H

#include "network-controller-components.h"
#include "network-status.h"
#include "lora-device-address.h"

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
 * LinkAdrRequest commands management.
 *
 * Supports two modes:
 *   - Standard ADR (ADR-AVG / ADR-MAX / ADR-MIN): SNR-margin based, uses packet history.
 *   - ADR-Lite: Binary search based, no packet history required.
 *
 * ADR-Lite Algorithm (Algorithm 1):
 *   K = {I_1, ..., I_|K|} sorted ascending by Energy Consumption (EC)
 *   I_k = {SF_k, TP_k, CF_k, CR_k}
 *   k_u(0) = |K| (most robust config)
 *   For each received packet at iteration t:
 *     if r_u(t) == k_u(t-1):  min_u=0, max_u=k_u(t-1)     // success
 *     else:                    min_u=k_u(t-1), max_u=|K|-1  // failure
 *     k_u(t) = floor((max_u + min_u) / 2)                   // binary search
 */
class AdrComponent : public NetworkControllerComponent
{
    /**
     * Available policies for combining radio metrics in packet history.
     */
    enum CombiningMethod
    {
        AVERAGE,
        MAXIMUM,
        MINIMUM,
        LITE,
    };

  public:
    /**
     *  Register this type.
     *  @return The object TypeId.
     */
    static TypeId GetTypeId();

    AdrComponent();           //!< Default constructor
    ~AdrComponent() override; //!< Destructor

    void OnReceivedPacket(Ptr<const Packet> packet,
                          Ptr<EndDeviceStatus> status,
                          Ptr<NetworkStatus> networkStatus) override;

    void BeforeSendingReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus) override;

    void OnFailedReply(Ptr<EndDeviceStatus> status, Ptr<NetworkStatus> networkStatus) override;

  private:
    //////////////////////////////////////////
    // Standard ADR (AVG / MAX / MIN) methods
    //////////////////////////////////////////

    /**
     * Implementation of the default Adaptive Data Rate (ADR) procedure.
     *
     * ADR is meant to optimize radio modulation parameters of end devices to improve energy
     * consuption and radio resource utilization. For more details see
     * https://doi.org/10.1109/NOMS.2018.8406255 .
     *
     * @param newDataRate [out] new data rate value selected for the end device.
     * @param newTxPower [out] new tx power [dBm] value selected for the end device.
     * @param status State representation of the current end device.
     */
    void AdrImplementation(uint8_t* newDataRate, double* newTxPower, Ptr<EndDeviceStatus> status);

    /**
     * Convert spreading factor values [7:12] to respective data rate values [0:5].
     *
     * @param sf The spreading factor value.
     * @return Value of the data rate as uint8_t.
     */
    uint8_t SfToDr(uint8_t sf);

    /**
     * Convert reception power values [dBm] to Signal to Noise Ratio (SNR) values [dB].
     *
     * The conversion comes from the formula \f$P_{rx}=-174+10\log_{10}(B)+SNR+NF\f$ where
     * \f$P_{rx}\f$ is the received transmission power, \f$B\f$ is the transmission bandwidth and
     * \f$NF\f$ is the noise figure of the receiver. The constant \f$-174\f$ is the thermal noise
     * [dBm] in 1 Hz of bandwidth and is influenced the temperature of the receiver, assumed
     * constant in this model. For more details see the SX1301 chip datasheet.
     *
     * @param transmissionPower Value of received transmission power.
     * @return SNR value as double.
     */
    double RxPowerToSNR(double transmissionPower) const;

    /**
     * Get the min RSSI (dBm) among gateways receiving the same transmission.
     *
     * @param gwList List of gateways paired with reception information.
     * @return Min RSSI of transmission as double.
     */
    double GetMinTxFromGateways(EndDeviceStatus::GatewayList gwList);
    /**
     * Get the max RSSI (dBm) among gateways receiving the same transmission.
     *
     * @param gwList List of gateways paired with packet reception information.
     * @return Max RSSI of transmission as double.
     */
    double GetMaxTxFromGateways(EndDeviceStatus::GatewayList gwList);
    /**
     * Get the average RSSI (dBm) of gateways receiving the same transmission.
     *
     * @param gwList List of gateways paired with packet reception information.
     * @return Average RSSI of transmission as double.
     */
    double GetAverageTxFromGateways(EndDeviceStatus::GatewayList gwList);
    /**
     * Get RSSI metric for a transmission according to chosen gateway aggregation policy.
     *
     * @param gwList List of gateways paired with packet reception information.
     * @return RSSI of tranmsmission as double.
     */
    double GetReceivedPower(EndDeviceStatus::GatewayList gwList);

    /**
     * Get the min Signal to Noise Ratio (SNR) of the receive packet history.
     *
     * @param packetList History of received packets with reception information.
     * @param historyRange Number of packets to consider going back in time.
     * @return Min SNR among packets as double.
     */
    double GetMinSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange);
    /**
     * Get the max Signal to Noise Ratio (SNR) of the receive packet history.
     *
     * @param packetList History of received packets with reception information.
     * @param historyRange Number of packets to consider going back in time.
     * @return Max SNR among packets as double.
     */
    double GetMaxSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange);
    /**
     * Get the average Signal to Noise Ratio (SNR) of the received packet history.
     *
     * @param packetList History of received packets with reception information.
     * @param historyRange Number of packets to consider going back in time.
     * @return Average SNR of packets as double.
     */
    double GetAverageSNR(EndDeviceStatus::ReceivedPacketList packetList, int historyRange);

    /**
     * Get the LoRaWAN protocol TxPower parameter from the Equivalent Radiated Power (ERP) in dBm.
     *
     * @param txPower Transission ERP configuration [dBm].
     * @return TxPower parameter value as uint8_t.
     */
    uint8_t GetTxPowerIndex(double txPower);

    //////////////////////////////////////////
    // ADR-Lite specific types and methods
    //////////////////////////////////////////

    /**
     * A LoRaWAN transmission configuration I_k = {SF_k, TP_k, CF_k, CR_k}.
     * Sorted ascending by Energy Consumption (EC).
     */
    struct LiteConfiguration
    {
        uint8_t sf;          //!< SF_k: Spreading Factor (7-12)
        double txPowerDbm;   //!< TP_k: Transmission power in dBm (2-14)
        uint8_t channelFreq; //!< CF_k: Channel frequency index (0, 1, 2)
        uint8_t codingRate;  //!< CR_k: Coding rate (1=4/5, 2=4/6, 3=4/7, 4=4/8)
        double energyIndex;  //!< EC_k: Relative energy consumption index

        bool operator<(const LiteConfiguration& other) const
        {
            return energyIndex < other.energyIndex;
        }
    };

    /**
     * Per-device state for ADR-Lite binary search.
     */
    struct DeviceAdrLiteState
    {
        int currentConfigIndex;      //!< k_u(t-1): Current assigned configuration index
        bool initialized;            //!< Whether the device has been initialized
        uint8_t lastAssignedSf;      //!< Last assigned SF_k
        double lastAssignedTxPower;  //!< Last assigned TP_k
        uint8_t lastAssignedCF;      //!< Last assigned CF_k (channel index)
        uint8_t lastAssignedCR;      //!< Last assigned CR_k (coding rate)
    };

    /**
     * Initialize the ADR-Lite configuration space K sorted by energy consumption.
     */
    void InitializeLiteConfigurationSpace();

    /**
     * Calculate Time on Air for a given SF and CR.
     *
     * @param sf Spreading factor (7-12)
     * @param payloadBytes Payload size in bytes
     * @param cr Coding rate (1-4)
     * @return Time on Air in milliseconds
     */
    double CalculateToA(uint8_t sf, int payloadBytes, uint8_t cr) const;

    /**
     * Calculate energy consumption index EC(I_k) for one configuration.
     *
     * @param sf Spreading factor
     * @param txPowerDbm Transmission power in dBm
     * @param cr Coding rate (1-4)
     * @return Energy consumption index (arbitrary units, for ordering)
     */
    double CalculateEnergyIndex(uint8_t sf, double txPowerDbm, uint8_t cr) const;

    /**
     * Get or create the per-device ADR-Lite state.
     *
     * @param deviceAddress The device address
     * @return Reference to the device state
     */
    DeviceAdrLiteState& GetDeviceLiteState(LoraDeviceAddress deviceAddress);

    /**
     * ADR-Lite binary search core.
     *
     * @param newConfigIndex [out] New configuration index k_u(t)
     * @param status End device status
     * @return true if parameters differ from previous assignment
     */
    bool AdrLiteImplementation(int* newConfigIndex, Ptr<EndDeviceStatus> status);

    /**
     * Check whether the last received packet used the assigned configuration.
     *
     * @param status End device status
     * @param state Device ADR-Lite state
     * @return true if r_u(t) == k_u(t-1)
     */
    bool ReceivedMatchesAssigned(Ptr<EndDeviceStatus> status,
                                 const DeviceAdrLiteState& state) const;

    /**
     * Convert TxPower dBm to TxPowerIndex for ADR-Lite (allows odd values).
     *
     * @param txPowerDbm Power in dBm
     * @return TxPower index (0-7)
     */
    uint8_t GetTxPowerIndexLite(double txPowerDbm) const;

    //////////////////////////////////////////
    // Standard ADR member variables
    //////////////////////////////////////////

    enum CombiningMethod tpAveraging;      //!< TX power from gateways policy
    int historyRange;                      //!< Number of previous packets to consider
    enum CombiningMethod historyAveraging; //!< Received SNR history policy

    const int min_spreadingFactor = 7;    //!< Spreading factor lower limit
    const int min_transmissionPower = 2;  //!< Minimum transmission power (dBm) (Europe)
    const int max_transmissionPower = 14; //!< Maximum transmission power (dBm) (Europe)
    // const int offset = 10;                //!< Device specific SNR margin (dB)
    const int B = 125000; //!< Bandwidth (Hz)
    const int NF = 6;     //!< Noise Figure (dB)
    double threshold[6] = {
        -20.0,
        -17.5,
        -15.0,
        -12.5,
        -10.0,
        -7.5}; //!< Vector containing the required SNR for the 6 allowed spreading factor
               //!< levels ranging from 7 to 12 (the SNR values are in dB).

    bool m_toggleTxPower; //!< Whether to control transmission power of end devices or not

    //////////////////////////////////////////
    // ADR-Lite member variables
    //////////////////////////////////////////

    bool m_useAdrLite;  //!< true = ADR-Lite mode, false = standard ADR

    std::vector<LiteConfiguration> m_liteConfigurations; //!< K: sorted configuration space
    std::map<LoraDeviceAddress, DeviceAdrLiteState> m_deviceLiteStates; //!< Per-device state

    int m_liteMinConfigIndex; //!< Index 0 (lowest energy)
    int m_liteMaxConfigIndex; //!< Index |K|-1 (highest energy / most robust)

    bool m_toggleCodingRate; //!< Whether ADR-Lite adjusts coding rate (CR_k)
    bool m_toggleChannel;    //!< Whether ADR-Lite adjusts channel frequency (CF_k)

    // LoRa PHY constants for ToA calculation
    const int m_liteBandwidth = 125000;    //!< Bandwidth in Hz
    const int m_litePreambleSymbols = 8;   //!< Preamble symbols
    const int m_litePayloadBytes = 20;     //!< Default payload size for ToA ordering
    const bool m_liteHeaderEnabled = true; //!< Explicit header mode
};
} // namespace lorawan
} // namespace ns3

#endif
