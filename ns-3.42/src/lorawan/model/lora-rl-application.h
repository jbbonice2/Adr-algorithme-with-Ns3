/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Unified RL LoRa application that interfaces with any RL agent.
 */

#ifndef LORA_RL_APPLICATION_H
#define LORA_RL_APPLICATION_H

#include "dlora-agent.h"
#include "lora-rl-packet-tag.h"
#include "qoca-agent.h"
#include "tow-agent.h"
#include "ucb1-agent.h"

#include "ns3/application.h"
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/end-device-lora-phy.h"
#include "ns3/event-id.h"
#include "ns3/lora-net-device.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <string>

namespace ns3
{
namespace lorawan
{

/**
 * \brief Unified application for RL-based LoRaWAN parameter selection.
 *
 * This application periodically sends packets through the LoRaWAN MAC/PHY
 * stack, using a pluggable RL agent to choose SF, TP, BW, and/or channel.
 * It supports D-LoRa, UCB1-Tuned, QoC-A, DQoC-A, and ToW agents, as well
 * as baseline modes (ADR, Random).
 *
 * The simulation script is responsible for connecting gateway PHY trace
 * sources (ReceivedPacket, LostPacketBecauseInterference, etc.) to
 * callbacks that ultimately invoke NotifyOutcome().
 */
class LoraRlApplication : public Application
{
  public:
    /**
     * Supported algorithm modes.
     */
    enum AlgoMode
    {
        DLORA,     ///< D-LoRa (UCB MAB separate SF/BW/TP arms)
        UCB1,      ///< UCB1-Tuned joint (SF,TP) selection
        QOCA,      ///< QoC-A / DQoC-A channel selection
        TOW,       ///< Tug-of-War channel + SF selection
        ADR,       ///< Fallback: use MAC-level ADR settings
        RANDOM     ///< Fallback: uniform random parameter selection
    };

    static TypeId GetTypeId();

    LoraRlApplication();

    /**
     * \brief Set the algorithm mode and optionally a subtype string.
     * \param mode Algorithm mode enum.
     * \param subtype Free-form label (e.g. "DLoRa-EE", "DQoC-A").
     */
    void SetAlgoMode(AlgoMode mode, const std::string& subtype = "");

    /**
     * \brief Provide a pre-configured D-LoRa agent.
     */
    void SetDLoRaAgent(Ptr<DLoRaAgent> agent);

    /**
     * \brief Provide a pre-configured UCB1 agent.
     */
    void SetUcb1Agent(Ptr<Ucb1Agent> agent);

    /**
     * \brief Provide a pre-configured QoC-A agent.
     */
    void SetQoCaAgent(Ptr<QoCaAgent> agent);

    /**
     * \brief Provide a pre-configured ToW agent.
     */
    void SetToWAgent(Ptr<ToWAgent> agent);

    /**
     * \brief Called by gateway callbacks to notify outcome.
     * \param success true if the packet was received by the gateway.
     */
    void NotifyOutcome(bool success);

    uint32_t GetPacketsSent() const;
    uint32_t GetPacketsReceived() const;

  protected:
    void StartApplication() override;
    void StopApplication() override;

  private:
    void SendPacket();

    // Attributes
    Time m_interval;
    uint32_t m_packetSize;

    // Counters
    uint32_t m_packetsSent;
    uint32_t m_packetsReceived;
    uint32_t m_sequenceNumber;
    EventId m_sendEvent;

    // LoRa device handles
    Ptr<LoraNetDevice> m_loraNetDevice;
    Ptr<ClassAEndDeviceLorawanMac> m_mac;
    Ptr<EndDeviceLoraPhy> m_phy;

    // Algorithm selection
    AlgoMode m_mode;
    std::string m_subtype;

    // Agents (only one is active at a time)
    Ptr<DLoRaAgent> m_dloraAgent;
    Ptr<Ucb1Agent> m_ucb1Agent;
    Ptr<QoCaAgent> m_qocaAgent;
    Ptr<ToWAgent> m_towAgent;

    // Last-used parameters for reward feedback
    int m_lastSF;
    double m_lastBW;
    double m_lastTP;
    uint32_t m_lastChannel;
};

} // namespace lorawan
} // namespace ns3

#endif // LORA_RL_APPLICATION_H
