/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Unified RL LoRa application implementation.
 */

#include "lora-rl-application.h"

#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <cmath>
#include <iomanip>
#include <iostream>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("LoraRlApplication");
NS_OBJECT_ENSURE_REGISTERED(LoraRlApplication);

// EU868 parameter sets
static const std::vector<int> SF_SET = {7, 8, 9, 10, 11, 12};
static const std::vector<double> BW_SET = {125000.0};
static const std::vector<double> TP_SET = {2.0, 5.0, 8.0, 11.0, 14.0};

TypeId
LoraRlApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::LoraRlApplication")
            .SetGroupName("lorawan")
            .AddConstructor<LoraRlApplication>()
            .SetParent<Application>()
            .AddAttribute("Interval",
                          "Time interval between packets",
                          TimeValue(Seconds(60.0)),
                          MakeTimeAccessor(&LoraRlApplication::m_interval),
                          MakeTimeChecker())
            .AddAttribute("PacketSize",
                          "Payload size in bytes",
                          UintegerValue(50),
                          MakeUintegerAccessor(&LoraRlApplication::m_packetSize),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

LoraRlApplication::LoraRlApplication()
    : m_packetsSent(0),
      m_packetsReceived(0),
      m_sequenceNumber(0),
      m_mode(ADR),
      m_lastSF(7),
      m_lastBW(125000.0),
      m_lastTP(14.0),
      m_lastChannel(0)
{
}

void
LoraRlApplication::SetAlgoMode(AlgoMode mode, const std::string& subtype)
{
    m_mode = mode;
    m_subtype = subtype;

    // Create default agents when none has been explicitly set
    switch (m_mode)
    {
    case DLORA:
        if (!m_dloraAgent)
        {
            m_dloraAgent = CreateObject<DLoRaAgent>();
        }
        break;
    case UCB1:
        if (!m_ucb1Agent)
        {
            m_ucb1Agent = CreateObject<Ucb1Agent>();
        }
        break;
    case QOCA:
        if (!m_qocaAgent)
        {
            m_qocaAgent = CreateObject<QoCaAgent>();
        }
        break;
    case TOW:
        if (!m_towAgent)
        {
            m_towAgent = CreateObject<ToWAgent>();
        }
        break;
    default:
        break;
    }
}

void
LoraRlApplication::SetDLoRaAgent(Ptr<DLoRaAgent> agent)
{
    m_dloraAgent = agent;
}

void
LoraRlApplication::SetUcb1Agent(Ptr<Ucb1Agent> agent)
{
    m_ucb1Agent = agent;
}

void
LoraRlApplication::SetQoCaAgent(Ptr<QoCaAgent> agent)
{
    m_qocaAgent = agent;
}

void
LoraRlApplication::SetToWAgent(Ptr<ToWAgent> agent)
{
    m_towAgent = agent;
}

uint32_t
LoraRlApplication::GetPacketsSent() const
{
    return m_packetsSent;
}

uint32_t
LoraRlApplication::GetPacketsReceived() const
{
    return m_packetsReceived;
}

void
LoraRlApplication::StartApplication()
{
    m_loraNetDevice = DynamicCast<LoraNetDevice>(GetNode()->GetDevice(0));

    if (m_loraNetDevice)
    {
        m_mac = DynamicCast<ClassAEndDeviceLorawanMac>(m_loraNetDevice->GetMac());
        m_phy = DynamicCast<EndDeviceLoraPhy>(m_loraNetDevice->GetPhy());
    }

    Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable>();
    double startDelay = rv->GetValue(0.0, m_interval.GetSeconds());
    m_sendEvent = Simulator::Schedule(Seconds(startDelay), &LoraRlApplication::SendPacket, this);
}

void
LoraRlApplication::StopApplication()
{
    Simulator::Cancel(m_sendEvent);
}

void
LoraRlApplication::SendPacket()
{
    if (!m_mac || !m_phy)
    {
        m_sendEvent =
            Simulator::Schedule(m_interval, &LoraRlApplication::SendPacket, this);
        return;
    }

    int sf = 7;
    double bw = 125000.0;
    double tp = 14.0;
    uint32_t channel = 0;

    switch (m_mode)
    {
    case DLORA: {
        auto params = m_dloraAgent->SelectParameters();
        sf = std::get<0>(params);
        bw = std::get<1>(params);
        tp = std::get<2>(params);
        break;
    }
    case UCB1: {
        auto params = m_ucb1Agent->SelectParameters();
        sf = params.first;
        tp = params.second;
        bw = 125000.0;
        break;
    }
    case QOCA: {
        channel = m_qocaAgent->SelectChannel();
        // QoC-A only selects channel; SF/TP can be fixed or combined with another agent
        sf = 7;
        tp = 14.0;
        bw = 125000.0;
        break;
    }
    case TOW: {
        auto params = m_towAgent->SelectParameters();
        channel = params.first;
        uint32_t sfIdx = params.second;
        sf = SF_SET[sfIdx];
        tp = TP_SET[2]; // 8 dBm default as in original
        bw = 125000.0;
        break;
    }
    case ADR: {
        sf = 12 - m_mac->GetDataRate();
        bw = 125000.0;
        tp = m_mac->GetTransmissionPowerDbm();
        break;
    }
    case RANDOM: {
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        sf = SF_SET[rng->GetInteger(0, SF_SET.size() - 1)];
        bw = BW_SET[rng->GetInteger(0, BW_SET.size() - 1)];
        tp = TP_SET[rng->GetInteger(0, TP_SET.size() - 1)];
        break;
    }
    }

    // Apply SF → DataRate conversion (DR5 = SF7, DR0 = SF12)
    uint8_t dataRate = static_cast<uint8_t>(12 - sf);
    if (dataRate > 5)
    {
        dataRate = 5;
    }
    m_mac->SetDataRate(dataRate);
    m_mac->SetTransmissionPowerDbm(tp);

    // Create packet with unified tag
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_sequenceNumber++;

    LoraRlPacketTag tag(GetNode()->GetId(),
                        m_sequenceNumber,
                        Simulator::Now().GetNanoSeconds(),
                        sf,
                        tp,
                        channel);
    packet->AddPacketTag(tag);

    m_mac->Send(packet);
    m_packetsSent++;

    m_lastSF = sf;
    m_lastBW = bw;
    m_lastTP = tp;
    m_lastChannel = channel;

    NS_LOG_INFO("[Device " << GetNode()->GetId() << "] Sent seq=" << m_sequenceNumber
                           << " SF=" << sf << " TP=" << tp << " CH=" << channel);

    // Schedule next transmission
    m_sendEvent =
        Simulator::Schedule(m_interval, &LoraRlApplication::SendPacket, this);
}

void
LoraRlApplication::NotifyOutcome(bool success)
{
    if (success)
    {
        m_packetsReceived++;
    }

    switch (m_mode)
    {
    case DLORA:
        m_dloraAgent->UpdateRewards(m_lastSF, m_lastBW, m_lastTP, success);
        break;
    case UCB1:
        m_ucb1Agent->UpdateRewards(m_lastSF, m_lastTP, success);
        break;
    case QOCA:
        m_qocaAgent->UpdateReward(m_lastChannel, success, success ? 1.0 : 0.0);
        break;
    case TOW: {
        uint32_t sfIdx = static_cast<uint32_t>(m_lastSF - 7);
        m_towAgent->UpdateReward(m_lastChannel, sfIdx, success);
        break;
    }
    default:
        break;
    }
}

} // namespace lorawan
} // namespace ns3
