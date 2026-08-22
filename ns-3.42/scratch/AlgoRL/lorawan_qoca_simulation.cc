/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * QoC-A and DQoC-A LoRaWAN Simulation using ns-3 LoRaWAN Module
 * Updated to use modular components from src/lorawan/model/
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include "ns3/lora-helper.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/lora-channel.h"
#include "ns3/lora-net-device.h"
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/end-device-lora-phy.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/lora-device-address-generator.h"
#include "ns3/forwarder-helper.h"
#include "ns3/network-server-helper.h"
#include "ns3/lora-packet-tracker.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"

// RL module components
#include "ns3/lora-rl-packet-tag.h"
#include "ns3/qoca-agent.h"
#include "ns3/lora-rl-application.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("LoRaWANQoCSimulation");

// ============================================================================
// GLOBAL METRICS
// ============================================================================

struct PacketInfo
{
    uint32_t nodeId;
    uint32_t sequenceNumber;
    Time sendTime;
    Time receiveTime;
    uint32_t channel;
    bool received;
};

struct PacketKey
{
    uint32_t nodeId;
    uint32_t seqNum;
    bool operator<(const PacketKey& other) const
    {
        if (nodeId != other.nodeId) return nodeId < other.nodeId;
        return seqNum < other.seqNum;
    }
};

std::map<PacketKey, PacketInfo> g_packetMap;
uint32_t g_totalSent = 0;
uint32_t g_totalReceived = 0;
uint32_t g_totalCollisions = 0;
uint32_t g_totalUnderSensitivity = 0;
std::vector<double> g_delays;

NodeContainer* g_endDevicesPtr = nullptr;

// ============================================================================
// METRICS
// ============================================================================

struct SimulationMetrics
{
    double pdr;
    double throughput;
    double avgDelay;
    double minDelay;
    double maxDelay;
    double jitter;
    double totalEnergyMJ;
    double energyEfficiency;
    uint32_t packetsSent;
    uint32_t packetsReceived;
    uint32_t packetsLostCollision;
    uint32_t packetsLostSensitivity;
};

SimulationMetrics CalculateMetrics(double simulationTime, uint32_t payloadSize)
{
    SimulationMetrics m{};
    m.packetsSent = g_totalSent;
    m.packetsReceived = g_totalReceived;
    m.packetsLostCollision = g_totalCollisions;
    m.packetsLostSensitivity = g_totalUnderSensitivity;
    m.pdr = (g_totalSent > 0) ? (double(g_totalReceived) / g_totalSent) * 100.0 : 0.0;
    double bitsReceived = g_totalReceived * payloadSize * 8.0;
    m.throughput = bitsReceived / simulationTime;

    if (!g_delays.empty())
    {
        double sum = 0; m.minDelay = g_delays[0]; m.maxDelay = g_delays[0];
        for (double d : g_delays) { sum += d; if (d < m.minDelay) m.minDelay = d; if (d > m.maxDelay) m.maxDelay = d; }
        m.avgDelay = sum / g_delays.size();
        double var = 0;
        for (double d : g_delays) var += (d - m.avgDelay) * (d - m.avgDelay);
        m.jitter = std::sqrt(var / g_delays.size());
    }

    double avgToAMs = 150.0;
    m.totalEnergyMJ = g_totalSent * avgToAMs * 0.05;
    m.energyEfficiency = (m.totalEnergyMJ > 0) ? bitsReceived / m.totalEnergyMJ : 0.0;
    return m;
}

void PrintResults(const SimulationMetrics& m, uint32_t numNodes, double duration, const std::string& algName)
{
    std::cout << "\n╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║           QoC-A/DQoC-A Simulation Results                ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║   Algorithm: " << std::setw(43) << std::left << algName << "║" << std::endl;
    std::cout << "║   Nodes: " << std::setw(47) << numNodes << "║" << std::endl;
    std::cout << "║   Duration: " << std::setw(41) << std::to_string(int(duration)) + " s" << "║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║   Packets Sent: " << std::setw(40) << m.packetsSent << "║" << std::endl;
    std::cout << "║   Packets Received: " << std::setw(36) << m.packetsReceived << "║" << std::endl;
    std::cout << "║   Lost (Collision): " << std::setw(36) << m.packetsLostCollision << "║" << std::endl;
    std::cout << "║   Lost (Sensitivity): " << std::setw(34) << m.packetsLostSensitivity << "║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "║   PDR: " << std::setw(46) << std::to_string(m.pdr).substr(0,5) + " %" << "║" << std::endl;
    std::cout << "║   Throughput: " << std::setw(39) << std::to_string(m.throughput).substr(0,8) + " bps" << "║" << std::endl;
    std::cout << "║   Avg Delay: " << std::setw(40) << std::to_string(m.avgDelay).substr(0,6) + " s" << "║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
}

// ============================================================================
// AGENT NOTIFICATION
// ============================================================================

void NotifyAgent(uint32_t nodeId, bool success)
{
    if (!g_endDevicesPtr) return;
    for (auto it = g_endDevicesPtr->Begin(); it != g_endDevicesPtr->End(); ++it)
    {
        if ((*it)->GetId() == nodeId)
        {
            for (uint32_t i = 0; i < (*it)->GetNApplications(); ++i)
            {
                auto app = DynamicCast<LoraRlApplication>((*it)->GetApplication(i));
                if (app) { app->NotifyOutcome(success); return; }
            }
        }
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void OnGatewayReceiveCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        PacketKey key = {tag.GetNodeId(), tag.GetSequenceNumber()};
        if (g_packetMap.count(key))
        {
            g_packetMap[key].receiveTime = Simulator::Now();
            g_packetMap[key].received = true;
            g_totalReceived++;
            double delay = (Simulator::Now() - g_packetMap[key].sendTime).GetSeconds();
            g_delays.push_back(delay);
            NotifyAgent(tag.GetNodeId(), true);
            NS_LOG_INFO("RECEIVED node=" << tag.GetNodeId() << " ch=" << tag.GetChannel() << " delay=" << delay);
        }
        else { g_totalReceived++; }
    }
}

void OnGatewayInterferenceCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    g_totalCollisions++;
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag)) NotifyAgent(tag.GetNodeId(), false);
}

void OnGatewayUnderSensitivityCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    g_totalUnderSensitivity++;
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag)) NotifyAgent(tag.GetNodeId(), false);
}

void OnEndDeviceStartSendingCallback(Ptr<const Packet> packet, uint32_t nodeId)
{
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        PacketKey key = {tag.GetNodeId(), tag.GetSequenceNumber()};
        if (!g_packetMap.count(key))
        {
            PacketInfo info;
            info.nodeId = tag.GetNodeId();
            info.sequenceNumber = tag.GetSequenceNumber();
            info.sendTime = Simulator::Now();
            info.channel = tag.GetChannel();
            info.received = false;
            g_packetMap[key] = info;
            g_totalSent++;
        }
    }
}

// ============================================================================
// TRACES
// ============================================================================

void ConnectTraces(NodeContainer& endDevices, NodeContainer& gateways)
{
    g_endDevicesPtr = &endDevices;
    for (auto it = gateways.Begin(); it != gateways.End(); ++it)
    {
        auto dev = DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (dev)
        {
            auto gwPhy = DynamicCast<GatewayLoraPhy>(dev->GetPhy());
            if (gwPhy)
            {
                gwPhy->TraceConnectWithoutContext("ReceivedPacket", MakeCallback(&OnGatewayReceiveCallback));
                gwPhy->TraceConnectWithoutContext("LostPacketBecauseInterference", MakeCallback(&OnGatewayInterferenceCallback));
                gwPhy->TraceConnectWithoutContext("LostPacketBecauseUnderSensitivity", MakeCallback(&OnGatewayUnderSensitivityCallback));
            }
        }
    }
    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        auto dev = DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (dev)
        {
            auto edPhy = DynamicCast<EndDeviceLoraPhy>(dev->GetPhy());
            if (edPhy) edPhy->TraceConnectWithoutContext("StartSending", MakeCallback(&OnEndDeviceStartSendingCallback));
        }
    }
}

void DisconnectTraces(NodeContainer& endDevices, NodeContainer& gateways)
{
    for (auto it = gateways.Begin(); it != gateways.End(); ++it)
    {
        auto dev = DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (dev)
        {
            auto gwPhy = DynamicCast<GatewayLoraPhy>(dev->GetPhy());
            if (gwPhy)
            {
                gwPhy->TraceDisconnectWithoutContext("ReceivedPacket", MakeCallback(&OnGatewayReceiveCallback));
                gwPhy->TraceDisconnectWithoutContext("LostPacketBecauseInterference", MakeCallback(&OnGatewayInterferenceCallback));
                gwPhy->TraceDisconnectWithoutContext("LostPacketBecauseUnderSensitivity", MakeCallback(&OnGatewayUnderSensitivityCallback));
            }
        }
    }
    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        auto dev = DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (dev)
        {
            auto edPhy = DynamicCast<EndDeviceLoraPhy>(dev->GetPhy());
            if (edPhy) edPhy->TraceDisconnectWithoutContext("StartSending", MakeCallback(&OnEndDeviceStartSendingCallback));
        }
    }
    g_endDevicesPtr = nullptr;
}

void ResetGlobals()
{
    g_totalSent = 0; g_totalReceived = 0;
    g_totalCollisions = 0; g_totalUnderSensitivity = 0;
    g_delays.clear(); g_packetMap.clear();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[])
{
    uint32_t numNodes = 10;
    double simulationTime = 600;
    uint32_t payloadSize = 50;
    double packetIntervalMinutes = 5;
    uint32_t mobilityPercentage = 0;
    double mobilitySpeed = 5;
    double radius = 1000;
    std::string algorithm = "QoC-A";
    std::string outputFile = "";
    std::string scenario = "S1_Density";

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of end devices", numNodes);
    cmd.AddValue("simulationTime", "Simulation time (s)", simulationTime);
    cmd.AddValue("payloadSize", "Payload size (bytes)", payloadSize);
    cmd.AddValue("packetInterval", "Packet interval (minutes)", packetIntervalMinutes);
    cmd.AddValue("mobilityPercentage", "Mobile nodes %", mobilityPercentage);
    cmd.AddValue("mobilitySpeed", "Mobile speed (m/s)", mobilitySpeed);
    cmd.AddValue("radius", "Network radius (m)", radius);
    cmd.AddValue("algorithm", "Algorithm (QoC-A or DQoC-A)", algorithm);
    cmd.AddValue("outputFile", "Output CSV path", outputFile);
    cmd.AddValue("scenario", "Scenario name", scenario);
    cmd.Parse(argc, argv);

    double packetInterval = packetIntervalMinutes * 60.0;

    LogComponentEnable("LoRaWANQoCSimulation", LOG_LEVEL_INFO);
    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnableAll(LOG_PREFIX_TIME);
    LogComponentEnableAll(LOG_PREFIX_NODE);

    // Channel
    auto loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);
    auto shadowing = CreateObject<CorrelatedShadowingPropagationLossModel>();
    loss->SetNext(shadowing);
    auto delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    auto channel = CreateObject<LoraChannel>(loss, delayModel);

    // Mobility
    MobilityHelper mobilityED;
    auto posAlloc = CreateObject<UniformDiscPositionAllocator>();
    posAlloc->SetX(0); posAlloc->SetY(0); posAlloc->SetRho(radius);
    mobilityED.SetPositionAllocator(posAlloc);

    MobilityHelper mobilityGW;
    auto gwPos = CreateObject<ListPositionAllocator>();
    gwPos->Add(Vector(0, 0, 15));
    mobilityGW.SetPositionAllocator(gwPos);
    mobilityGW.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Helpers
    LoraPhyHelper phyHelper;
    phyHelper.SetChannel(channel);
    LorawanMacHelper macHelper;
    macHelper.SetRegion(LorawanMacHelper::EU);
    LoraHelper helper;
    helper.EnablePacketTracking();

    // End devices
    NodeContainer endDevices;
    endDevices.Create(numNodes);

    uint32_t numMobile = (uint32_t)std::round(numNodes * mobilityPercentage / 100.0);
    if (numMobile > 0 && numMobile < numNodes)
    {
        NodeContainer mobNodes, statNodes;
        for (uint32_t i = 0; i < numNodes; ++i)
            (i < numMobile ? mobNodes : statNodes).Add(endDevices.Get(i));
        MobilityHelper mm; mm.SetPositionAllocator(posAlloc);
        std::ostringstream ss; ss << "ns3::ConstantRandomVariable[Constant=" << mobilitySpeed << "]";
        mm.SetMobilityModel("ns3::RandomWalk2dMobilityModel","Mode",StringValue("Time"),"Time",StringValue("2s"),
            "Speed",StringValue(ss.str()),"Bounds",RectangleValue(Rectangle(-radius,radius,-radius,radius)));
        mm.Install(mobNodes);
        MobilityHelper ms; ms.SetPositionAllocator(posAlloc);
        ms.SetMobilityModel("ns3::ConstantPositionMobilityModel"); ms.Install(statNodes);
    }
    else
    {
        mobilityED.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobilityED.Install(endDevices);
    }

    auto addrGen = CreateObject<LoraDeviceAddressGenerator>(uint8_t(54), uint32_t(1864));
    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    macHelper.SetAddressGenerator(addrGen);
    helper.Install(phyHelper, macHelper, endDevices);

    // Gateway
    NodeContainer gateways; gateways.Create(1);
    mobilityGW.Install(gateways);
    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);

    LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);

    // Install LoraRlApplication with QoCA agent
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        auto app = CreateObject<LoraRlApplication>();
        app->SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        app->SetAttribute("PacketSize", UintegerValue(payloadSize));

        auto agent = CreateObject<QoCaAgent>();
        if (algorithm == "DQoC-A")
            agent->SetAlgorithmType(QoCaAgent::DQOC_A);
        else
            agent->SetAlgorithmType(QoCaAgent::QOC_A);
        app->SetQoCaAgent(agent);
        app->SetAlgoMode(LoraRlApplication::QOCA, algorithm);

        endDevices.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(0));
        app->SetStopTime(Seconds(simulationTime));
    }

    // Network server
    auto nsNode = CreateObject<Node>();
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    P2PGwRegistration_t gwReg;
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        auto c = p2p.Install(nsNode, *gw);
        gwReg.emplace_back(DynamicCast<PointToPointNetDevice>(c.Get(0)), *gw);
    }
    NetworkServerHelper nsh; nsh.SetGatewaysP2P(gwReg); nsh.SetEndDevices(endDevices); nsh.Install(nsNode);
    ForwarderHelper fh; fh.Install(gateways);

    ConnectTraces(endDevices, gateways);

    // Run
    NS_LOG_INFO("QoC-A simulation: " << algorithm << " " << numNodes << " nodes");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Results
    auto metrics = CalculateMetrics(simulationTime, payloadSize);
    PrintResults(metrics, numNodes, simulationTime, algorithm);

    LoraPacketTracker& tracker = helper.GetPacketTracker();
    std::cout << "  PHY: " << tracker.PrintPhyPacketsPerGw(Seconds(0), Seconds(simulationTime), gateways.Get(0)->GetId()) << std::endl;

    if (!outputFile.empty())
    {
        std::ofstream csv(outputFile);
        csv << "Scenario,NumDevices,Algorithm,PacketsSent,PacketsReceived,PDR,Throughput,AvgDelay,EnergyEfficiency\n";
        csv << scenario << "," << numNodes << "," << algorithm << ","
            << metrics.packetsSent << "," << metrics.packetsReceived << ","
            << metrics.pdr << "," << metrics.throughput << ","
            << metrics.avgDelay << "," << metrics.energyEfficiency << "\n";
        csv.close();
        std::cout << "Results saved to: " << outputFile << std::endl;
    }

    DisconnectTraces(endDevices, gateways);
    ResetGlobals();
    return 0;
}
