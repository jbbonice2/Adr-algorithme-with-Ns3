/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Unified packet tag for RL-based LoRaWAN simulations.
 * Carries metadata (nodeId, seqNum, sendTime, SF, TP, channel) across
 * PHY/MAC layers where the ns-3 packet UID changes due to header operations.
 */

#ifndef LORA_RL_PACKET_TAG_H
#define LORA_RL_PACKET_TAG_H

#include "ns3/tag.h"

#include <cstdint>

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * Packet tag used by RL-based parameter selection algorithms
 * (D-LoRa, UCB1-Tuned, QoC-A, DQoC-A, ToW).
 *
 * The tag survives header add/remove operations in the LoRaWAN stack,
 * enabling end-to-end packet tracking from ED application to GW PHY.
 */
class LoraRlPacketTag : public Tag
{
  public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    LoraRlPacketTag();
    LoraRlPacketTag(uint32_t nodeId,
                    uint32_t seqNum,
                    int64_t sendTimeNs,
                    int sf,
                    double tp,
                    uint32_t channel);

    // Tag interface
    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer i) const override;
    void Deserialize(TagBuffer i) override;
    void Print(std::ostream& os) const override;

    // Accessors
    uint32_t GetNodeId() const;
    uint32_t GetSequenceNumber() const;
    int64_t GetSendTimeNs() const;
    int GetSF() const;
    double GetTP() const;
    uint32_t GetChannel() const;

  private:
    uint32_t m_nodeId;        //!< Originating node ID
    uint32_t m_sequenceNumber; //!< Application-level sequence number
    int64_t m_sendTime;        //!< Send timestamp in nanoseconds
    int m_sf;                  //!< Spreading Factor used (7-12)
    double m_tp;               //!< Transmission power used (dBm)
    uint32_t m_channel;        //!< Channel index used
};

} // namespace lorawan
} // namespace ns3

#endif // LORA_RL_PACKET_TAG_H
