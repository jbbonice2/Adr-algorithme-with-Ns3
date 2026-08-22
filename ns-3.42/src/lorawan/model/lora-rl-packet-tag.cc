/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Unified packet tag for RL-based LoRaWAN simulations.
 */

#include "lora-rl-packet-tag.h"

#include "ns3/log.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("LoraRlPacketTag");

TypeId
LoraRlPacketTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::LoraRlPacketTag")
                            .SetParent<Tag>()
                            .SetGroupName("lorawan")
                            .AddConstructor<LoraRlPacketTag>();
    return tid;
}

TypeId
LoraRlPacketTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

LoraRlPacketTag::LoraRlPacketTag()
    : m_nodeId(0),
      m_sequenceNumber(0),
      m_sendTime(0),
      m_sf(7),
      m_tp(14.0),
      m_channel(0)
{
}

LoraRlPacketTag::LoraRlPacketTag(uint32_t nodeId,
                                 uint32_t seqNum,
                                 int64_t sendTimeNs,
                                 int sf,
                                 double tp,
                                 uint32_t channel)
    : m_nodeId(nodeId),
      m_sequenceNumber(seqNum),
      m_sendTime(sendTimeNs),
      m_sf(sf),
      m_tp(tp),
      m_channel(channel)
{
}

uint32_t
LoraRlPacketTag::GetSerializedSize() const
{
    // 4 + 4 + 8 + 4 + 8 + 4 = 32 bytes
    return 32;
}

void
LoraRlPacketTag::Serialize(TagBuffer i) const
{
    i.WriteU32(m_nodeId);
    i.WriteU32(m_sequenceNumber);
    i.WriteU64(static_cast<uint64_t>(m_sendTime));
    i.WriteU32(static_cast<uint32_t>(m_sf));
    i.WriteDouble(m_tp);
    i.WriteU32(m_channel);
}

void
LoraRlPacketTag::Deserialize(TagBuffer i)
{
    m_nodeId = i.ReadU32();
    m_sequenceNumber = i.ReadU32();
    m_sendTime = static_cast<int64_t>(i.ReadU64());
    m_sf = static_cast<int>(i.ReadU32());
    m_tp = i.ReadDouble();
    m_channel = i.ReadU32();
}

void
LoraRlPacketTag::Print(std::ostream& os) const
{
    os << "NodeId=" << m_nodeId << ", SeqNum=" << m_sequenceNumber
       << ", SF=" << m_sf << ", TP=" << m_tp << ", CH=" << m_channel;
}

uint32_t
LoraRlPacketTag::GetNodeId() const
{
    return m_nodeId;
}

uint32_t
LoraRlPacketTag::GetSequenceNumber() const
{
    return m_sequenceNumber;
}

int64_t
LoraRlPacketTag::GetSendTimeNs() const
{
    return m_sendTime;
}

int
LoraRlPacketTag::GetSF() const
{
    return m_sf;
}

double
LoraRlPacketTag::GetTP() const
{
    return m_tp;
}

uint32_t
LoraRlPacketTag::GetChannel() const
{
    return m_channel;
}

} // namespace lorawan
} // namespace ns3
