/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Unified RL LoRaWAN Simulation using ns-3 LoRaWAN Module
 *
 * Supports: DLoRa, UCB1-Tuned, QoC-A, DQoC-A, ToW, ADR, Random
 * Uses modular components from src/lorawan/model/
 */

#include "ns3/core-module.h"
#include "ns3/energy-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

// LoRaWAN module
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"
#include "ns3/end-device-lora-phy.h"
#include "ns3/forwarder-helper.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/lora-channel.h"
#include "ns3/lora-device-address-generator.h"
#include "ns3/lora-helper.h"
#include "ns3/lora-net-device.h"
#include "ns3/lora-packet-tracker.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/network-server-helper.h"

// RL module components
#include "ns3/dlora-agent.h"
#include "ns3/lora-rl-application.h"
#include "ns3/lora-rl-packet-tag.h"
#include "ns3/qoca-agent.h"
#include "ns3/tow-agent.h"
#include "ns3/ucb1-agent.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("UnifiedRLSimulation");

// ---------------------------------------------------------------------------
// Global metrics
// ---------------------------------------------------------------------------
struct PacketInfo
{
    uint32_t nodeId;
    uint32_t sequenceNumber;
    Time sendTime;
    Time receiveTime;
    int sf;
    double tp;
    uint32_t channel;
    bool received;
};

struct PacketKey
{
    uint32_t nodeId;
    uint32_t seqNum;

    bool operator<(const PacketKey& other) const
    {
        if (nodeId != other.nodeId)
            return nodeId < other.nodeId;
        return seqNum < other.seqNum;
    }
};

static std::map<PacketKey, PacketInfo> g_packetMap;
static uint32_t g_totalSent = 0;
static uint32_t g_totalReceived = 0;
static uint32_t g_totalCollisions = 0;
static uint32_t g_totalUnderSensitivity = 0;
static std::vector<double> g_delays;
static NodeContainer* g_endDevicesPtr = nullptr;

// ---------------------------------------------------------------------------
// Metrics structure
// ---------------------------------------------------------------------------
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
    double avgEnergyPerPacket;
    uint32_t packetsSent;
    uint32_t packetsReceived;
    uint32_t packetsLostCollision;
    uint32_t packetsLostSensitivity;
};

// ---------------------------------------------------------------------------
// Notify helpers
// ---------------------------------------------------------------------------
static void
NotifyAgent(uint32_t nodeId, bool success)
{
    if (!g_endDevicesPtr)
        return;
    for (auto it = g_endDevicesPtr->Begin(); it != g_endDevicesPtr->End(); ++it)
    {
        if ((*it)->GetId() == nodeId)
        {
            for (uint32_t i = 0; i < (*it)->GetNApplications(); ++i)
            {
                auto app = DynamicCast<LoraRlApplication>((*it)->GetApplication(i));
                if (app)
                {
                    app->NotifyOutcome(success);
                    return;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Gateway callbacks
// ---------------------------------------------------------------------------
static void
OnGatewayReceiveCallback(Ptr<const Packet> packet, uint32_t gwId)
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
            double delay =
                (Simulator::Now() - g_packetMap[key].sendTime).GetSeconds();
            g_delays.push_back(delay);
            NotifyAgent(tag.GetNodeId(), true);
            NS_LOG_INFO("RECEIVED node=" << tag.GetNodeId()
                                         << " seq=" << tag.GetSequenceNumber()
                                         << " delay=" << delay << "s");
        }
        else
        {
            g_totalReceived++;
        }
    }
}

static void
OnGatewayInterferenceCallback(Ptr<const Packet> packet, uint32_t /*gwId*/)
{
    g_totalCollisions++;
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        NotifyAgent(tag.GetNodeId(), false);
    }
}

static void
OnGatewayUnderSensitivityCallback(Ptr<const Packet> packet, uint32_t /*gwId*/)
{
    g_totalUnderSensitivity++;
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        NotifyAgent(tag.GetNodeId(), false);
    }
}

static void
OnEndDeviceStartSendingCallback(Ptr<const Packet> packet, uint32_t /*nodeId*/)
{
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        // Record sent packet in tracking map
        PacketKey key = {tag.GetNodeId(), tag.GetSequenceNumber()};
        if (!g_packetMap.count(key))
        {
            PacketInfo info;
            info.nodeId = tag.GetNodeId();
            info.sequenceNumber = tag.GetSequenceNumber();
            info.sendTime = Simulator::Now();
            info.sf = tag.GetSF();
            info.tp = tag.GetTP();
            info.channel = tag.GetChannel();
            info.received = false;
            g_packetMap[key] = info;
            g_totalSent++;
        }
    }
}

// ---------------------------------------------------------------------------
// Trace connect / disconnect
// ---------------------------------------------------------------------------
static void
ConnectTraces(NodeContainer& endDevices, NodeContainer& gateways)
{
    g_endDevicesPtr = &endDevices;

    for (auto it = gateways.Begin(); it != gateways.End(); ++it)
    {
        auto loraDevice =
            DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (loraDevice)
        {
            auto gwPhy =
                DynamicCast<GatewayLoraPhy>(loraDevice->GetPhy());
            if (gwPhy)
            {
                gwPhy->TraceConnectWithoutContext(
                    "ReceivedPacket",
                    MakeCallback(&OnGatewayReceiveCallback));
                gwPhy->TraceConnectWithoutContext(
                    "LostPacketBecauseInterference",
                    MakeCallback(&OnGatewayInterferenceCallback));
                gwPhy->TraceConnectWithoutContext(
                    "LostPacketBecauseUnderSensitivity",
                    MakeCallback(&OnGatewayUnderSensitivityCallback));
            }
        }
    }

    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        auto loraDevice =
            DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (loraDevice)
        {
            auto edPhy =
                DynamicCast<EndDeviceLoraPhy>(loraDevice->GetPhy());
            if (edPhy)
            {
                edPhy->TraceConnectWithoutContext(
                    "StartSending",
                    MakeCallback(&OnEndDeviceStartSendingCallback));
            }
        }
    }
}

static void
DisconnectTraces(NodeContainer& endDevices, NodeContainer& gateways)
{
    for (auto it = gateways.Begin(); it != gateways.End(); ++it)
    {
        auto loraDevice =
            DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (loraDevice)
        {
            auto gwPhy =
                DynamicCast<GatewayLoraPhy>(loraDevice->GetPhy());
            if (gwPhy)
            {
                gwPhy->TraceDisconnectWithoutContext(
                    "ReceivedPacket",
                    MakeCallback(&OnGatewayReceiveCallback));
                gwPhy->TraceDisconnectWithoutContext(
                    "LostPacketBecauseInterference",
                    MakeCallback(&OnGatewayInterferenceCallback));
                gwPhy->TraceDisconnectWithoutContext(
                    "LostPacketBecauseUnderSensitivity",
                    MakeCallback(&OnGatewayUnderSensitivityCallback));
            }
        }
    }

    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        auto loraDevice =
            DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (loraDevice)
        {
            auto edPhy =
                DynamicCast<EndDeviceLoraPhy>(loraDevice->GetPhy());
            if (edPhy)
            {
                edPhy->TraceDisconnectWithoutContext(
                    "StartSending",
                    MakeCallback(&OnEndDeviceStartSendingCallback));
            }
        }
    }

    g_endDevicesPtr = nullptr;
}

// ---------------------------------------------------------------------------
// Metrics calculation
// ---------------------------------------------------------------------------
static SimulationMetrics
CalculateMetrics(double simulationTime,
                 uint32_t payloadSize,
                 EnergySourceContainer& sources,
                 bool energyEnabled)
{
    SimulationMetrics m{};
    m.packetsSent = g_totalSent;
    m.packetsReceived = g_totalReceived;
    m.packetsLostCollision = g_totalCollisions;
    m.packetsLostSensitivity = g_totalUnderSensitivity;

    m.pdr = (g_totalSent > 0)
                ? (double(g_totalReceived) / g_totalSent) * 100.0
                : 0.0;

    double bitsReceived = g_totalReceived * payloadSize * 8.0;
    m.throughput = bitsReceived / simulationTime;

    if (!g_delays.empty())
    {
        double sum = 0;
        m.minDelay = g_delays[0];
        m.maxDelay = g_delays[0];
        for (double d : g_delays)
        {
            sum += d;
            if (d < m.minDelay)
                m.minDelay = d;
            if (d > m.maxDelay)
                m.maxDelay = d;
        }
        m.avgDelay = sum / g_delays.size();

        double var = 0;
        for (double d : g_delays)
        {
            var += (d - m.avgDelay) * (d - m.avgDelay);
        }
        m.jitter = std::sqrt(var / g_delays.size());
    }

    if (energyEnabled && sources.GetN() > 0)
    {
        double totalE = 0;
        for (uint32_t i = 0; i < sources.GetN(); ++i)
        {
            totalE += (10000.0 - sources.Get(i)->GetRemainingEnergy());
        }
        m.totalEnergyMJ = totalE * 1000.0;
        m.energyEfficiency =
            (m.totalEnergyMJ > 0) ? bitsReceived / m.totalEnergyMJ : 0.0;
        m.avgEnergyPerPacket =
            (g_totalReceived > 0) ? m.totalEnergyMJ / g_totalReceived : 0.0;
    }

    return m;
}

static void
PrintMetrics(const SimulationMetrics& m,
             const std::string& algo,
             uint32_t numNodes,
             double radius,
             double duration)
{
    std::cout << "\n=== " << algo << " Simulation Results ===" << std::endl;
    std::cout << "  Nodes: " << numNodes << "  Radius: " << radius
              << " m  Duration: " << duration << " s" << std::endl;
    std::cout << "  Sent: " << m.packetsSent
              << "  Received: " << m.packetsReceived
              << "  Collision: " << m.packetsLostCollision
              << "  UnderSens: " << m.packetsLostSensitivity << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  PDR: " << m.pdr << " %"
              << "  Throughput: " << m.throughput << " bps" << std::endl;
    std::cout << "  AvgDelay: " << m.avgDelay << " s"
              << "  Jitter: " << m.jitter << " s" << std::endl;
    std::cout << "  Energy: " << m.totalEnergyMJ << " mJ"
              << "  Efficiency: " << m.energyEfficiency << " bits/mJ"
              << std::endl;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int
main(int argc, char* argv[])
{
    uint32_t numNodes = 50;
    double simulationTime = 3600.0;
    double topologyRadius = 2000.0;
    std::string algorithm = "DLoRa";
    double packetInterval = 60.0;
    uint32_t payloadSize = 20;
    bool enablePrinting = false;
    bool enableEnergyModel = true;
    uint32_t mobilityPercentage = 0;
    double mobilitySpeed = 0.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of LoRa end devices", numNodes);
    cmd.AddValue("simulationTime", "Simulation time (s)", simulationTime);
    cmd.AddValue("topologyRadius", "Topology radius (m)", topologyRadius);
    cmd.AddValue("algorithm",
                 "Algorithm: DLoRa, DLoRa-PDR, DLoRa-EE, DLoRa-TH, "
                 "UCB1, QoC-A, DQoC-A, ToW, ADR, Random",
                 algorithm);
    cmd.AddValue("packetInterval", "Tx interval (s)", packetInterval);
    cmd.AddValue("payloadSize", "Payload size (bytes)", payloadSize);
    cmd.AddValue("enablePrinting", "Enable periodic stats", enablePrinting);
    cmd.AddValue("enableEnergyModel", "Enable energy model", enableEnergyModel);
    cmd.AddValue("mobilityPercentage", "Mobile nodes (%)", mobilityPercentage);
    cmd.AddValue("mobilitySpeed", "Mobile speed (km/h)", mobilitySpeed);
    cmd.Parse(argc, argv);

    double mobilitySpeedMs = mobilitySpeed / 3.6;

    // ---- Channel ----
    Ptr<LogDistancePropagationLossModel> loss =
        CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<CorrelatedShadowingPropagationLossModel> shadowing =
        CreateObject<CorrelatedShadowingPropagationLossModel>();
    loss->SetNext(shadowing);

    Ptr<PropagationDelayModel> delay =
        CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    // ---- Mobility ----
    MobilityHelper mobilityED;
    auto posAlloc = CreateObject<UniformDiscPositionAllocator>();
    posAlloc->SetX(0.0);
    posAlloc->SetY(0.0);
    posAlloc->SetRho(topologyRadius);
    mobilityED.SetPositionAllocator(posAlloc);

    MobilityHelper mobilityGW;
    auto gwPos = CreateObject<ListPositionAllocator>();
    gwPos->Add(Vector(0.0, 0.0, 15.0));
    mobilityGW.SetPositionAllocator(gwPos);
    mobilityGW.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // ---- Helpers ----
    LoraPhyHelper phyHelper;
    phyHelper.SetChannel(channel);

    LorawanMacHelper macHelper;
    macHelper.SetRegion(LorawanMacHelper::EU);

    LoraHelper helper;
    helper.EnablePacketTracking();

    // ---- End devices ----
    NodeContainer endDevices;
    endDevices.Create(numNodes);

    uint32_t numMobile =
        static_cast<uint32_t>(std::round(numNodes * mobilityPercentage / 100.0));

    if (numMobile > 0 && numMobile < numNodes)
    {
        NodeContainer mobileNodes, staticNodes;
        for (uint32_t i = 0; i < numNodes; ++i)
        {
            if (i < numMobile)
                mobileNodes.Add(endDevices.Get(i));
            else
                staticNodes.Add(endDevices.Get(i));
        }

        MobilityHelper mobMobile;
        mobMobile.SetPositionAllocator(posAlloc);
        std::ostringstream ss;
        ss << "ns3::ConstantRandomVariable[Constant=" << mobilitySpeedMs << "]";
        mobMobile.SetMobilityModel(
            "ns3::RandomWalk2dMobilityModel",
            "Mode", StringValue("Time"),
            "Time", StringValue("2s"),
            "Speed", StringValue(ss.str()),
            "Bounds",
            RectangleValue(Rectangle(-topologyRadius, topologyRadius,
                                     -topologyRadius, topologyRadius)));
        mobMobile.Install(mobileNodes);

        MobilityHelper mobStatic;
        mobStatic.SetPositionAllocator(posAlloc);
        mobStatic.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobStatic.Install(staticNodes);
    }
    else if (numMobile > 0)
    {
        std::ostringstream ss;
        ss << "ns3::ConstantRandomVariable[Constant=" << mobilitySpeedMs << "]";
        mobilityED.SetMobilityModel(
            "ns3::RandomWalk2dMobilityModel",
            "Mode", StringValue("Time"),
            "Time", StringValue("2s"),
            "Speed", StringValue(ss.str()),
            "Bounds",
            RectangleValue(Rectangle(-topologyRadius, topologyRadius,
                                     -topologyRadius, topologyRadius)));
        mobilityED.Install(endDevices);
    }
    else
    {
        mobilityED.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobilityED.Install(endDevices);
    }

    // LoRa on end devices
    uint8_t nwkId = 54;
    uint32_t nwkAddr = 1864;
    auto addrGen = CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddr);

    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    macHelper.SetAddressGenerator(addrGen);
    NetDeviceContainer edDevices =
        helper.Install(phyHelper, macHelper, endDevices);

    // ---- Gateway ----
    NodeContainer gateways;
    gateways.Create(1);
    mobilityGW.Install(gateways);

    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);

    // ---- Initial SF assignment ----
    LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);

    // ---- Energy model ----
    EnergySourceContainer sources;
    if (enableEnergyModel)
    {
        BasicEnergySourceHelper srcHelper;
        srcHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000));
        srcHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.3));

        LoraRadioEnergyModelHelper radioHelper;
        radioHelper.Set("StandbyCurrentA", DoubleValue(0.0014));
        radioHelper.Set("TxCurrentA", DoubleValue(0.028));
        radioHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
        radioHelper.Set("RxCurrentA", DoubleValue(0.0112));
        radioHelper.SetTxCurrentModel("ns3::ConstantLoraTxCurrentModel",
                                      "TxCurrent", DoubleValue(0.028));

        sources = srcHelper.Install(endDevices);
        radioHelper.Install(edDevices, sources);
    }

    // ---- Determine mode ----
    LoraRlApplication::AlgoMode mode;
    std::string subtype = algorithm;

    if (algorithm == "DLoRa" || algorithm == "DLoRa-PDR" ||
        algorithm == "DLoRa-EE" || algorithm == "DLoRa-TH")
    {
        mode = LoraRlApplication::DLORA;
    }
    else if (algorithm == "UCB1")
    {
        mode = LoraRlApplication::UCB1;
    }
    else if (algorithm == "QoC-A")
    {
        mode = LoraRlApplication::QOCA;
    }
    else if (algorithm == "DQoC-A")
    {
        mode = LoraRlApplication::QOCA;
    }
    else if (algorithm == "ToW")
    {
        mode = LoraRlApplication::TOW;
    }
    else if (algorithm == "ADR")
    {
        mode = LoraRlApplication::ADR;
    }
    else
    {
        mode = LoraRlApplication::RANDOM;
    }

    // ---- Install applications ----
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        auto app = CreateObject<LoraRlApplication>();
        app->SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        app->SetAttribute("PacketSize", UintegerValue(payloadSize));

        // Configure agent for the chosen mode
        if (mode == LoraRlApplication::DLORA)
        {
            auto agent = CreateObject<DLoRaAgent>();
            if (algorithm == "DLoRa")
            {
                agent->SetVariantWeights(0.0, 0.0, 1.8);
            }
            else if (algorithm == "DLoRa-EE")
            {
                agent->SetVariantWeights(0.0, 0.0, 3.5);
            }
            else if (algorithm == "DLoRa-TH")
            {
                agent->SetVariantWeights(10.0, 10.0, 0.0);
            }
            // DLoRa-PDR: weights stay at 0
            app->SetDLoRaAgent(agent);
        }
        else if (mode == LoraRlApplication::QOCA)
        {
            auto agent = CreateObject<QoCaAgent>();
            if (algorithm == "DQoC-A")
            {
                agent->SetAlgorithmType(QoCaAgent::DQOC_A);
            }
            else
            {
                agent->SetAlgorithmType(QoCaAgent::QOC_A);
            }
            app->SetQoCaAgent(agent);
        }

        app->SetAlgoMode(mode, subtype);
        endDevices.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(0));
        app->SetStopTime(Seconds(simulationTime));
    }

    // ---- Network server ----
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

    NetworkServerHelper nsHelper;
    nsHelper.SetGatewaysP2P(gwReg);
    nsHelper.SetEndDevices(endDevices);
    nsHelper.Install(nsNode);

    ForwarderHelper fwdHelper;
    fwdHelper.Install(gateways);

    // ---- Traces ----
    ConnectTraces(endDevices, gateways);

    if (enablePrinting)
    {
        helper.EnablePeriodicDeviceStatusPrinting(
            endDevices, gateways, "device-status-" + algorithm + ".txt", Seconds(60));
        helper.EnablePeriodicPhyPerformancePrinting(
            gateways, "phy-performance-" + algorithm + ".txt", Seconds(60));
        helper.EnablePeriodicGlobalPerformancePrinting(
            "global-performance-" + algorithm + ".txt", Seconds(60));
    }

    // ---- Run ----
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // ---- Results ----
    SimulationMetrics metrics =
        CalculateMetrics(simulationTime, payloadSize, sources, enableEnergyModel);
    PrintMetrics(metrics, algorithm, numNodes, topologyRadius, simulationTime);

    // PHY/MAC comparison stats
    LoraPacketTracker& tracker = helper.GetPacketTracker();
    std::cout << "  PHY stats: "
              << tracker.PrintPhyPacketsPerGw(
                     Seconds(0), Seconds(simulationTime), gateways.Get(0)->GetId())
              << std::endl;
    std::cout << "  MAC stats: "
              << tracker.CountMacPacketsGlobally(
                     Seconds(0), Seconds(simulationTime))
              << std::endl;

    // Write CSV
    std::string csvName =
        "results_" + algorithm + "_" + std::to_string(numNodes) + "nodes.csv";
    std::ofstream csv(csvName);
    if (csv.is_open())
    {
        csv << "Algorithm,NumNodes,Radius,Duration,PacketsSent,PacketsReceived,"
            << "PDR,Throughput_bps,AvgDelay_s,MinDelay_s,MaxDelay_s,Jitter_s,"
            << "TotalEnergy_mJ,EnergyEfficiency_bits_per_mJ,AvgEnergyPerPacket_mJ,"
            << "CollisionLoss,SensitivityLoss\n";
        csv << std::fixed << std::setprecision(6);
        csv << algorithm << "," << numNodes << "," << topologyRadius << ","
            << simulationTime << "," << metrics.packetsSent << ","
            << metrics.packetsReceived << "," << metrics.pdr << ","
            << metrics.throughput << "," << metrics.avgDelay << ","
            << metrics.minDelay << "," << metrics.maxDelay << ","
            << metrics.jitter << "," << metrics.totalEnergyMJ << ","
            << metrics.energyEfficiency << "," << metrics.avgEnergyPerPacket << ","
            << metrics.packetsLostCollision << "," << metrics.packetsLostSensitivity
            << "\n";
        csv.close();
    }

    // Per-packet CSV
    std::string pktName =
        "packets_" + algorithm + "_" + std::to_string(numNodes) + "nodes.csv";
    std::ofstream pktCsv(pktName);
    if (pktCsv.is_open())
    {
        pktCsv << "NodeID,SeqNum,SF,TP,Channel,SendTime_s,ReceiveTime_s,Delay_s,"
                  "Received\n";
        for (const auto& [key, info] : g_packetMap)
        {
            pktCsv << info.nodeId << "," << info.sequenceNumber << ","
                   << info.sf << "," << info.tp << "," << info.channel << ","
                   << info.sendTime.GetSeconds() << ",";
            if (info.received)
            {
                pktCsv << info.receiveTime.GetSeconds() << ","
                       << (info.receiveTime - info.sendTime).GetSeconds()
                       << ",1";
            }
            else
            {
                pktCsv << "0,0,0";
            }
            pktCsv << "\n";
        }
        pktCsv.close();
    }

    DisconnectTraces(endDevices, gateways);
    g_packetMap.clear();
    g_delays.clear();

    return 0;
}
