/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * D-LoRa Simulation using ns-3 LoRaWAN Module
 * Updated to use modular components from src/lorawan/model/
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

// LoRaWAN module includes
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
#include "ns3/periodic-sender-helper.h"
#include "ns3/one-shot-sender-helper.h"
#include "ns3/forwarder-helper.h"
#include "ns3/network-server-helper.h"
#include "ns3/lora-packet-tracker.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"
#include "ns3/building-penetration-loss.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/file-helper.h"

// RL module components
#include "ns3/lora-rl-packet-tag.h"
#include "ns3/dlora-agent.h"
#include "ns3/lora-rl-application.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <map>
#include <vector>

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("DLoRaSimulation");

// ============================================================================
// GLOBAL METRICS
// ============================================================================

double g_totalEnergyConsumed = 0;

struct PacketInfo
{
    uint32_t nodeId;
    uint32_t sequenceNumber;
    Time sendTime;
    Time receiveTime;
    int sf;
    double bw;
    double tp;
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
    double avgEnergyPerPacket;
    uint32_t packetsSent;
    uint32_t packetsReceived;
    uint32_t packetsLostCollision;
    uint32_t packetsLostSensitivity;
};

SimulationMetrics CalculateMetrics(double simulationTime, uint32_t payloadSize,
                                   EnergySourceContainer& sources, bool energyEnabled)
{
    SimulationMetrics metrics;
    metrics.packetsSent = g_totalSent;
    metrics.packetsReceived = g_totalReceived;
    metrics.packetsLostCollision = g_totalCollisions;
    metrics.packetsLostSensitivity = g_totalUnderSensitivity;

    metrics.pdr = (g_totalSent > 0) ? (double(g_totalReceived) / g_totalSent) * 100.0 : 0.0;

    double bitsReceived = g_totalReceived * payloadSize * 8.0;
    metrics.throughput = bitsReceived / simulationTime;

    if (!g_delays.empty())
    {
        double sum = 0;
        metrics.minDelay = g_delays[0];
        metrics.maxDelay = g_delays[0];
        for (double d : g_delays)
        {
            sum += d;
            if (d < metrics.minDelay) metrics.minDelay = d;
            if (d > metrics.maxDelay) metrics.maxDelay = d;
        }
        metrics.avgDelay = sum / g_delays.size();
        double variance = 0;
        for (double d : g_delays)
            variance += (d - metrics.avgDelay) * (d - metrics.avgDelay);
        metrics.jitter = std::sqrt(variance / g_delays.size());
    }
    else
    {
        metrics.avgDelay = 0; metrics.minDelay = 0;
        metrics.maxDelay = 0; metrics.jitter = 0;
    }

    if (energyEnabled && sources.GetN() > 0)
    {
        double totalEnergy = 0;
        for (uint32_t i = 0; i < sources.GetN(); ++i)
            totalEnergy += (10000.0 - sources.Get(i)->GetRemainingEnergy());
        metrics.totalEnergyMJ = totalEnergy * 1000.0;
        metrics.energyEfficiency = (metrics.totalEnergyMJ > 0) ?
            (bitsReceived / metrics.totalEnergyMJ) : 0.0;
        metrics.avgEnergyPerPacket = (g_totalReceived > 0) ?
            (metrics.totalEnergyMJ / g_totalReceived) : 0.0;
    }
    else
    {
        metrics.totalEnergyMJ = 0; metrics.energyEfficiency = 0;
        metrics.avgEnergyPerPacket = 0;
    }
    return metrics;
}

void PrintDetailedMetrics(const SimulationMetrics& m, const std::string& algorithm,
                          uint32_t numNodes, double radius, double duration)
{
    std::cout << "\n╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║           D-LoRa Simulation Results                      ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ Configuration                                            ║" << std::endl;
    std::cout << "║   Algorithm: " << std::setw(43) << std::left << algorithm << "║" << std::endl;
    std::cout << "║   Nodes: " << std::setw(47) << numNodes << "║" << std::endl;
    std::cout << "║   Radius: " << std::setw(43) << std::to_string(int(radius)) + " m" << "║" << std::endl;
    std::cout << "║   Duration: " << std::setw(41) << std::to_string(int(duration)) + " s" << "║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ Packet Statistics                                        ║" << std::endl;
    std::cout << "║   Packets Sent: " << std::setw(40) << m.packetsSent << "║" << std::endl;
    std::cout << "║   Packets Received: " << std::setw(36) << m.packetsReceived << "║" << std::endl;
    std::cout << "║   Lost (Collision): " << std::setw(36) << m.packetsLostCollision << "║" << std::endl;
    std::cout << "║   Lost (Sensitivity): " << std::setw(34) << m.packetsLostSensitivity << "║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ Performance Metrics                                      ║" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "║   PDR: " << std::setw(46) << std::to_string(m.pdr).substr(0,5) + " %" << "║" << std::endl;
    std::cout << "║   Throughput: " << std::setw(39) << std::to_string(m.throughput).substr(0,8) + " bps" << "║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ Delay Metrics                                            ║" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "║   Average Delay: " << std::setw(37) << std::to_string(m.avgDelay).substr(0,8) + " s" << "║" << std::endl;
    std::cout << "║   Min Delay: " << std::setw(41) << std::to_string(m.minDelay).substr(0,8) + " s" << "║" << std::endl;
    std::cout << "║   Max Delay: " << std::setw(41) << std::to_string(m.maxDelay).substr(0,8) + " s" << "║" << std::endl;
    std::cout << "║   Jitter: " << std::setw(44) << std::to_string(m.jitter).substr(0,8) + " s" << "║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ Energy Metrics                                           ║" << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "║   Total Energy: " << std::setw(38) << std::to_string(m.totalEnergyMJ).substr(0,10) + " mJ" << "║" << std::endl;
    std::cout << "║   Energy Efficiency: " << std::setw(31) << std::to_string(m.energyEfficiency).substr(0,8) + " bits/mJ" << "║" << std::endl;
    std::cout << "║   Avg Energy/Packet: " << std::setw(33) << std::to_string(m.avgEnergyPerPacket).substr(0,8) + " mJ" << "║" << std::endl;
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
// GATEWAY CALLBACKS
// ============================================================================

void OnGatewayReceiveCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        PacketKey key = {tag.GetNodeId(), tag.GetSequenceNumber()};
        if (g_packetMap.find(key) != g_packetMap.end())
        {
            g_packetMap[key].receiveTime = Simulator::Now();
            g_packetMap[key].received = true;
            g_totalReceived++;
            double delay = (Simulator::Now() - g_packetMap[key].sendTime).GetSeconds();
            g_delays.push_back(delay);
            NotifyAgent(tag.GetNodeId(), true);

            NS_LOG_INFO("*** RECEIVED *** Node " << tag.GetNodeId()
                        << " seq=" << tag.GetSequenceNumber()
                        << " SF=" << tag.GetSF()
                        << " delay=" << delay << "s");
        }
        else
        {
            g_totalReceived++;
        }
    }
}

void OnGatewayInterferenceCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    g_totalCollisions++;
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        NotifyAgent(tag.GetNodeId(), false);
        NS_LOG_INFO("*** INTERFERENCE *** Node " << tag.GetNodeId()
                    << " seq=" << tag.GetSequenceNumber());
    }
}

void OnGatewayUnderSensitivityCallback(Ptr<const Packet> packet, uint32_t gwId)
{
    g_totalUnderSensitivity++;
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        NotifyAgent(tag.GetNodeId(), false);
        NS_LOG_INFO("*** UNDER SENSITIVITY *** Node " << tag.GetNodeId()
                    << " seq=" << tag.GetSequenceNumber());
    }
}

void OnEndDeviceStartSendingCallback(Ptr<const Packet> packet, uint32_t nodeId)
{
    LoraRlPacketTag tag;
    if (packet->PeekPacketTag(tag))
    {
        PacketKey key = {tag.GetNodeId(), tag.GetSequenceNumber()};
        if (g_packetMap.find(key) == g_packetMap.end())
        {
            PacketInfo info;
            info.nodeId = tag.GetNodeId();
            info.sequenceNumber = tag.GetSequenceNumber();
            info.sendTime = Simulator::Now();
            info.sf = tag.GetSF();
            info.bw = 125000;
            info.tp = tag.GetTP();
            info.received = false;
            g_packetMap[key] = info;
            g_totalSent++;
        }
    }
}

// ============================================================================
// TRACE CONNECTIONS
// ============================================================================

void ConnectTraces(NodeContainer& endDevices, NodeContainer& gateways)
{
    g_endDevicesPtr = &endDevices;

    for (auto it = gateways.Begin(); it != gateways.End(); ++it)
    {
        auto loraDevice = DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (loraDevice)
        {
            auto gwPhy = DynamicCast<GatewayLoraPhy>(loraDevice->GetPhy());
            if (gwPhy)
            {
                gwPhy->TraceConnectWithoutContext("ReceivedPacket",
                    MakeCallback(&OnGatewayReceiveCallback));
                gwPhy->TraceConnectWithoutContext("LostPacketBecauseInterference",
                    MakeCallback(&OnGatewayInterferenceCallback));
                gwPhy->TraceConnectWithoutContext("LostPacketBecauseUnderSensitivity",
                    MakeCallback(&OnGatewayUnderSensitivityCallback));
            }
        }
    }

    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        auto loraDevice = DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (loraDevice)
        {
            auto edPhy = DynamicCast<EndDeviceLoraPhy>(loraDevice->GetPhy());
            if (edPhy)
            {
                edPhy->TraceConnectWithoutContext("StartSending",
                    MakeCallback(&OnEndDeviceStartSendingCallback));
            }
        }
    }
}

void DisconnectTraces(NodeContainer& endDevices, NodeContainer& gateways)
{
    for (auto it = gateways.Begin(); it != gateways.End(); ++it)
    {
        auto loraDevice = DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (loraDevice)
        {
            auto gwPhy = DynamicCast<GatewayLoraPhy>(loraDevice->GetPhy());
            if (gwPhy)
            {
                gwPhy->TraceDisconnectWithoutContext("ReceivedPacket",
                    MakeCallback(&OnGatewayReceiveCallback));
                gwPhy->TraceDisconnectWithoutContext("LostPacketBecauseInterference",
                    MakeCallback(&OnGatewayInterferenceCallback));
                gwPhy->TraceDisconnectWithoutContext("LostPacketBecauseUnderSensitivity",
                    MakeCallback(&OnGatewayUnderSensitivityCallback));
            }
        }
    }

    for (auto it = endDevices.Begin(); it != endDevices.End(); ++it)
    {
        auto loraDevice = DynamicCast<LoraNetDevice>((*it)->GetDevice(0));
        if (loraDevice)
        {
            auto edPhy = DynamicCast<EndDeviceLoraPhy>(loraDevice->GetPhy());
            if (edPhy)
            {
                edPhy->TraceDisconnectWithoutContext("StartSending",
                    MakeCallback(&OnEndDeviceStartSendingCallback));
            }
        }
    }
    g_endDevicesPtr = nullptr;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[])
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
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("topologyRadius", "Radius of the network topology in meters", topologyRadius);
    cmd.AddValue("algorithm", "Algorithm: DLoRa, DLoRa-PDR, DLoRa-EE, DLoRa-TH, Random, ADR", algorithm);
    cmd.AddValue("packetInterval", "Packet transmission interval in seconds", packetInterval);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("enablePrinting", "Enable periodic status printing", enablePrinting);
    cmd.AddValue("enableEnergyModel", "Enable energy consumption tracking", enableEnergyModel);
    cmd.AddValue("mobilityPercentage", "Percentage of mobile end devices (0-100)", mobilityPercentage);
    cmd.AddValue("mobilitySpeed", "Speed of mobile devices in km/h", mobilitySpeed);
    cmd.Parse(argc, argv);

    double mobilitySpeedMs = mobilitySpeed / 3.6;

    // Logging
    LogComponentEnable("DLoRaSimulation", LOG_LEVEL_INFO);
    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnableAll(LOG_PREFIX_NODE);
    LogComponentEnableAll(LOG_PREFIX_TIME);

    // Channel
    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    Ptr<CorrelatedShadowingPropagationLossModel> shadowing =
        CreateObject<CorrelatedShadowingPropagationLossModel>();
    loss->SetNext(shadowing);

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();
    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);

    // Mobility
    MobilityHelper mobilityED;
    Ptr<UniformDiscPositionAllocator> positionAllocED = CreateObject<UniformDiscPositionAllocator>();
    positionAllocED->SetX(0.0);
    positionAllocED->SetY(0.0);
    positionAllocED->SetRho(topologyRadius);
    mobilityED.SetPositionAllocator(positionAllocED);

    MobilityHelper mobilityGW;
    Ptr<ListPositionAllocator> positionAllocGW = CreateObject<ListPositionAllocator>();
    positionAllocGW->Add(Vector(0.0, 0.0, 15.0));
    mobilityGW.SetPositionAllocator(positionAllocGW);
    mobilityGW.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Helpers
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);
    LorawanMacHelper macHelper = LorawanMacHelper();
    macHelper.SetRegion(LorawanMacHelper::EU);
    LoraHelper helper = LoraHelper();
    helper.EnablePacketTracking();

    // End Devices
    NodeContainer endDevices;
    endDevices.Create(numNodes);

    uint32_t numMobile = (uint32_t)std::round(numNodes * (mobilityPercentage / 100.0));

    if (numMobile > 0 && numMobile < numNodes)
    {
        NodeContainer mobileNodes, staticNodes;
        for (uint32_t i = 0; i < numNodes; ++i)
        {
            if (i < numMobile) mobileNodes.Add(endDevices.Get(i));
            else staticNodes.Add(endDevices.Get(i));
        }
        MobilityHelper mobilityMobile;
        mobilityMobile.SetPositionAllocator(positionAllocED);
        std::ostringstream speedStr;
        speedStr << "ns3::ConstantRandomVariable[Constant=" << mobilitySpeedMs << "]";
        mobilityMobile.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
            "Mode", StringValue("Time"), "Time", StringValue("2s"),
            "Speed", StringValue(speedStr.str()),
            "Bounds", RectangleValue(Rectangle(-topologyRadius, topologyRadius, -topologyRadius, topologyRadius)));
        mobilityMobile.Install(mobileNodes);
        MobilityHelper mobilityStatic;
        mobilityStatic.SetPositionAllocator(positionAllocED);
        mobilityStatic.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobilityStatic.Install(staticNodes);
    }
    else if (numMobile > 0)
    {
        std::ostringstream speedStr;
        speedStr << "ns3::ConstantRandomVariable[Constant=" << mobilitySpeedMs << "]";
        mobilityED.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
            "Mode", StringValue("Time"), "Time", StringValue("2s"),
            "Speed", StringValue(speedStr.str()),
            "Bounds", RectangleValue(Rectangle(-topologyRadius, topologyRadius, -topologyRadius, topologyRadius)));
        mobilityED.Install(endDevices);
    }
    else
    {
        mobilityED.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobilityED.Install(endDevices);
    }

    uint8_t nwkId = 54;
    uint32_t nwkAddr = 1864;
    Ptr<LoraDeviceAddressGenerator> addrGen =
        CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddr);

    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    macHelper.SetAddressGenerator(addrGen);
    NetDeviceContainer endDevicesNetDevices = helper.Install(phyHelper, macHelper, endDevices);

    // Gateway
    NodeContainer gateways;
    gateways.Create(1);
    mobilityGW.Install(gateways);
    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    NetDeviceContainer gatewayNetDevices = helper.Install(phyHelper, macHelper, gateways);

    // SF distribution
    std::vector<int> sfDistribution = LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);

    // Energy model
    EnergySourceContainer sources;
    if (enableEnergyModel)
    {
        BasicEnergySourceHelper basicSourceHelper;
        LoraRadioEnergyModelHelper radioEnergyHelper;
        basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000));
        basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.3));
        radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.0014));
        radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.028));
        radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
        radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0112));
        radioEnergyHelper.SetTxCurrentModel("ns3::ConstantLoraTxCurrentModel",
                                            "TxCurrent", DoubleValue(0.028));
        sources = basicSourceHelper.Install(endDevices);
        DeviceEnergyModelContainer deviceModels =
            radioEnergyHelper.Install(endDevicesNetDevices, sources);
    }

    // Install LoraRlApplication with DLoRa agent
    ApplicationContainer apps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<LoraRlApplication> app = CreateObject<LoraRlApplication>();
        app->SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        app->SetAttribute("PacketSize", UintegerValue(payloadSize));

        if (algorithm == "ADR")
        {
            app->SetAlgoMode(LoraRlApplication::ADR, algorithm);
        }
        else if (algorithm == "Random")
        {
            app->SetAlgoMode(LoraRlApplication::RANDOM, algorithm);
        }
        else
        {
            // D-LoRa variants
            Ptr<DLoRaAgent> agent = CreateObject<DLoRaAgent>();
            if (algorithm == "DLoRa")
                agent->SetVariantWeights(0, 0, 1.8);
            else if (algorithm == "DLoRa-PDR")
                agent->SetVariantWeights(0, 0, 0);
            else if (algorithm == "DLoRa-EE")
                agent->SetVariantWeights(0, 0, 3.5);
            else if (algorithm == "DLoRa-TH")
                agent->SetVariantWeights(10, 10, 0);
            app->SetDLoRaAgent(agent);
            app->SetAlgoMode(LoraRlApplication::DLORA, algorithm);
        }

        endDevices.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(0));
        app->SetStopTime(Seconds(simulationTime));
        apps.Add(app);
    }

    // Network Server
    Ptr<Node> networkServer = CreateObject<Node>();
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    P2PGwRegistration_t gwRegistration;
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw)
    {
        auto container = p2p.Install(networkServer, *gw);
        auto serverP2PNetDev = DynamicCast<PointToPointNetDevice>(container.Get(0));
        gwRegistration.emplace_back(serverP2PNetDev, *gw);
    }

    NetworkServerHelper networkServerHelper;
    networkServerHelper.SetGatewaysP2P(gwRegistration);
    networkServerHelper.SetEndDevices(endDevices);
    networkServerHelper.Install(networkServer);

    ForwarderHelper forwarderHelper;
    forwarderHelper.Install(gateways);

    // Connect traces
    ConnectTraces(endDevices, gateways);

    if (enablePrinting)
    {
        helper.EnablePeriodicDeviceStatusPrinting(endDevices, gateways,
            "device-status-" + algorithm + ".txt", Seconds(60));
        helper.EnablePeriodicPhyPerformancePrinting(gateways,
            "phy-performance-" + algorithm + ".txt", Seconds(60));
        helper.EnablePeriodicGlobalPerformancePrinting(
            "global-performance-" + algorithm + ".txt", Seconds(60));
    }

    // Run
    NS_LOG_INFO("Starting D-LoRa simulation: " << algorithm << " " << numNodes
                << " nodes, " << topologyRadius << " m, " << simulationTime << " s");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Results
    LoraPacketTracker& tracker = helper.GetPacketTracker();
    std::string phyResults = tracker.PrintPhyPacketsPerGw(Seconds(0), Seconds(simulationTime),
                                                          gateways.Get(0)->GetId());
    std::string macResults = tracker.CountMacPacketsGlobally(Seconds(0), Seconds(simulationTime));

    std::istringstream phyStream(phyResults);
    int totPacketsSent, receivedPackets, interferedPackets, noMoreGwPackets,
        underSensitivityPackets, lostBecauseTxPackets;
    phyStream >> totPacketsSent >> receivedPackets >> interferedPackets
              >> noMoreGwPackets >> underSensitivityPackets >> lostBecauseTxPackets;

    SimulationMetrics metrics = CalculateMetrics(simulationTime, payloadSize,
                                                  sources, enableEnergyModel);
    PrintDetailedMetrics(metrics, algorithm, numNodes, topologyRadius, simulationTime);

    std::cout << "\n--- Tracking Comparison ---" << std::endl;
    std::cout << "  Custom: Sent=" << g_totalSent << " Rcvd=" << g_totalReceived
              << " Coll=" << g_totalCollisions << " UnderSens=" << g_totalUnderSensitivity << std::endl;
    std::cout << "  PHY: Sent=" << totPacketsSent << " Rcvd=" << receivedPackets
              << " Interf=" << interferedPackets << " UnderSens=" << underSensitivityPackets << std::endl;

    // CSV output
    std::string csvFileName = "results_" + algorithm + "_" + std::to_string(numNodes) + "nodes.csv";
    std::ofstream csvFile(csvFileName);
    if (csvFile.is_open())
    {
        csvFile << "Algorithm,NumNodes,Radius,Duration,PacketsSent,PacketsReceived,"
                << "PDR,Throughput_bps,AvgDelay_s,MinDelay_s,MaxDelay_s,Jitter_s,"
                << "TotalEnergy_mJ,EnergyEfficiency_bits_per_mJ,AvgEnergyPerPacket_mJ,"
                << "CollisionLoss,SensitivityLoss" << std::endl;
        csvFile << std::fixed << std::setprecision(6);
        csvFile << algorithm << "," << numNodes << "," << topologyRadius << ","
                << simulationTime << "," << metrics.packetsSent << ","
                << metrics.packetsReceived << "," << metrics.pdr << ","
                << metrics.throughput << "," << metrics.avgDelay << ","
                << metrics.minDelay << "," << metrics.maxDelay << ","
                << metrics.jitter << "," << metrics.totalEnergyMJ << ","
                << metrics.energyEfficiency << "," << metrics.avgEnergyPerPacket << ","
                << metrics.packetsLostCollision << "," << metrics.packetsLostSensitivity
                << std::endl;
        csvFile.close();
    }

    // Per-packet CSV
    std::string perPacketFileName = "packets_" + algorithm + "_" + std::to_string(numNodes) + "nodes.csv";
    std::ofstream perPacketFile(perPacketFileName);
    if (perPacketFile.is_open())
    {
        perPacketFile << "NodeID,SeqNum,SF,BW,TP,SendTime_s,ReceiveTime_s,Delay_s,Received" << std::endl;
        for (const auto& [key, info] : g_packetMap)
        {
            perPacketFile << info.nodeId << "," << info.sequenceNumber << ","
                          << info.sf << "," << info.bw << "," << info.tp << ","
                          << info.sendTime.GetSeconds() << ",";
            if (info.received)
                perPacketFile << info.receiveTime.GetSeconds() << ","
                              << (info.receiveTime - info.sendTime).GetSeconds() << ",1";
            else
                perPacketFile << "0,0,0";
            perPacketFile << std::endl;
        }
        perPacketFile.close();
    }

    DisconnectTraces(endDevices, gateways);
    g_packetMap.clear();
    g_delays.clear();

    return 0;
}
