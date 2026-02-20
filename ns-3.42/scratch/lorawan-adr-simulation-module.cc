/*
 * LoRaWAN ADR Simulation using ns-3 LoRaWAN Module
 * 
 * This simulation is based on the experiment structure from lorawan-adr-simulationfinal.cc
 * but uses the official ns-3 lorawan module for PHY and MAC layers.
 * 
 * ADR algorithm is based on ns3::AdrComponent implementation.
 * Energy model is integrated from lorawan-energy-model-example.cc.
 * 
 * Author: Based on examples from ns-3 lorawan module
 */

#include "ns3/adr-lite-component.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/building-penetration-loss.h"
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/end-device-lorawan-mac.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"
#include "ns3/core-module.h"
#include "ns3/double.h"
#include "ns3/end-device-lora-phy.h"
#include "ns3/energy-module.h"
#include "ns3/forwarder-helper.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/log.h"
#include "ns3/lora-channel.h"
#include "ns3/lora-net-device.h"
#include "ns3/lora-phy.h"
#include "ns3/lora-device-address-generator.h"
#include "ns3/lora-helper.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lora-radio-energy-model-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/network-module.h"
#include "ns3/network-server-helper.h"
#include "ns3/node-container.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/periodic-sender.h"
#include "ns3/point-to-point-module.h"
#include "ns3/pointer.h"
#include "ns3/position-allocator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rectangle.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("LoRaWANADRSimulationModule");

// --- Global variables for tracking ---
std::map<uint32_t, double> g_initialEnergy;
std::map<uint32_t, Ptr<BasicEnergySource>> g_energySources;
uint32_t g_packetsSent = 0;
uint32_t g_packetsReceived = 0;
uint32_t g_packetsLost = 0;

// Global containers for device tracking
NodeContainer g_endDevices;
NodeContainer g_gateways;
std::map<uint32_t, uint32_t> g_packetToDevice;  // Maps packet UID to device ID
std::map<uint32_t, uint32_t> g_deviceMessageCount;  // Counts messages per device

/**
 * Log all device details (ID, position, mobility type)
 */
void
LogDeviceDetails(NodeContainer& endDevices, int numFixedNodes)
{
    NS_LOG_INFO("\n========== DEVICE CREATION SUMMARY ==========");
    NS_LOG_INFO("Total devices created: " << endDevices.GetN());
    NS_LOG_INFO("Fixed devices: " << numFixedNodes);
    NS_LOG_INFO("Mobile devices: " << (endDevices.GetN() - numFixedNodes));
    NS_LOG_INFO("\n--- Device Details ---");
    
    for (uint32_t i = 0; i < endDevices.GetN(); ++i) {
        Ptr<Node> node = endDevices.Get(i);
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        Vector pos = mobility->GetPosition();
        
        // Determine mobility type
        std::string mobilityType = "UNKNOWN";
        if (mobility->GetInstanceTypeId().GetName() == "ns3::ConstantPositionMobilityModel") {
            mobilityType = "FIXED";
        } else if (mobility->GetInstanceTypeId().GetName() == "ns3::RandomWalk2dMobilityModel") {
            mobilityType = "MOBILE";
        }
        
        NS_LOG_INFO("[DEVICE] ID=" << node->GetId() 
                    << " | Position=(" << std::fixed << std::setprecision(2) 
                    << pos.x << ", " << pos.y << ", " << pos.z << ")m"
                    << " | Mobility=" << mobilityType);
        
        // Initialize message counter
        g_deviceMessageCount[node->GetId()] = 0;
    }
    NS_LOG_INFO("============================================\n");
}

/**
 * Callback for packet transmission from end device
 * Note: For MakeBoundCallback, bound args come first
 */
void
OnEndDeviceSend(uint32_t deviceNodeId, Ptr<const Packet> packet)
{
    g_packetsSent++;
    g_deviceMessageCount[deviceNodeId]++;
    g_packetToDevice[packet->GetUid()] = deviceNodeId;
    
    // Get device position and mobility type
    std::string posStr = "N/A";
    std::string mobilityType = "UNKNOWN";
    
    // Communication parameters: I_k = {SF_k, TP_k, CF_k, CR_k}
    uint8_t sf = 0;
    double txPower = 0.0;
    double channelFreq = 0.0;
    uint8_t codingRate = 0;
    
    for (uint32_t i = 0; i < g_endDevices.GetN(); ++i) {
        if (g_endDevices.Get(i)->GetId() == deviceNodeId) {
            Ptr<Node> node = g_endDevices.Get(i);
            
            // Get mobility info
            Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
            if (mob) {
                Vector pos = mob->GetPosition();
                std::ostringstream oss;
                oss << "(" << std::fixed << std::setprecision(1) << pos.x << "," << pos.y << "," << pos.z << ")";
                posStr = oss.str();
                
                // Determine mobility type
                if (mob->GetInstanceTypeId().GetName() == "ns3::ConstantPositionMobilityModel") {
                    mobilityType = "FIXED";
                } else if (mob->GetInstanceTypeId().GetName() == "ns3::RandomWalk2dMobilityModel") {
                    mobilityType = "MOBILE";
                }
            }
            
            // Get communication parameters from MAC layer
            Ptr<LoraNetDevice> loraDevice = node->GetDevice(0)->GetObject<LoraNetDevice>();
            if (loraDevice) {
                Ptr<EndDeviceLorawanMac> mac = 
                    DynamicCast<EndDeviceLorawanMac>(loraDevice->GetMac());
                if (mac) {
                    // Get Data Rate and convert to SF (EU868: DR0=SF12, DR5=SF7)
                    uint8_t dr = mac->GetDataRate();
                    sf = 12 - dr;  // EU868 mapping
                    
                    // Get Transmission Power (TP_k)
                    txPower = mac->GetTransmissionPowerDbm();
                    
                    // Get Channel Frequency (CF_k)
                    channelFreq = mac->GetNextTxChannelFrequency() / 1e6;  // Hz to MHz
                    
                    // Get Coding Rate (CR_k)
                    codingRate = mac->GetCodingRate();
                }
            }
            break;
        }
    }
    
    NS_LOG_INFO("[TX] DeviceID=" << deviceNodeId 
                << " | MsgNum=" << g_deviceMessageCount[deviceNodeId]
                << " | PacketUID=" << packet->GetUid() 
                << " | SF=" << (int)sf
                << " | TP=" << std::fixed << std::setprecision(1) << txPower << "dBm"
                << " | CF=" << std::fixed << std::setprecision(1) << channelFreq << "MHz"
                << " | CR=4/" << (4 + codingRate)
                << " | Position=" << posStr
                << " | Mobility=" << mobilityType
                << " | Size=" << packet->GetSize() << "B"
                << " | TotalSent=" << g_packetsSent);
}

/**
 * Callback for packet transmission from end device PHY
 */
void
OnPhyTxStart(Ptr<const Packet> packet, uint32_t systemId)
{
    // This is called at PHY layer, systemId is the node ID
    NS_LOG_DEBUG("[PHY-TX] NodeID=" << systemId << " PacketUID=" << packet->GetUid());
}

/**
 * Callback for successful packet reception at gateway
 * Note: For MakeBoundCallback, bound args come first
 */
void
OnGatewayReceive(uint32_t gatewayNodeId, Ptr<const Packet> packet)
{
    g_packetsReceived++;
    
    // Find which device sent this packet
    uint32_t senderDeviceId = 0;
    auto it = g_packetToDevice.find(packet->GetUid());
    if (it != g_packetToDevice.end()) {
        senderDeviceId = it->second;
    }
    
    NS_LOG_INFO("[RX-GW] GatewayID=" << gatewayNodeId 
                << " | PacketUID=" << packet->GetUid()
                << " | FromDeviceID=" << senderDeviceId
                << " | Size=" << packet->GetSize() << "B" 
                << " | TotalReceived=" << g_packetsReceived
                << " | SUCCESS");
}

/**
 * Callback for successful packet reception at gateway (PHY level)
 */
void
OnPhyRxSuccess(Ptr<const Packet> packet, uint32_t systemId)
{
    NS_LOG_DEBUG("[PHY-RX] GatewayNodeID=" << systemId << " PacketUID=" << packet->GetUid());
}

/**
 * Callback for packet reception failure (interference)
 */
void
OnPhyRxInterference(Ptr<const Packet> packet, uint32_t systemId)
{
    g_packetsLost++;
    
    // Find sender device
    uint32_t senderDeviceId = 0;
    auto it = g_packetToDevice.find(packet->GetUid());
    if (it != g_packetToDevice.end()) {
        senderDeviceId = it->second;
    }
    
    NS_LOG_WARN("[RX-FAIL] GatewayID=" << systemId 
                << " | PacketUID=" << packet->GetUid()
                << " | FromDeviceID=" << senderDeviceId
                << " | Reason=INTERFERENCE"
                << " | TotalLost=" << g_packetsLost);
}

/**
 * Callback for packet under sensitivity
 */
void
OnPhyRxUnderSensitivity(Ptr<const Packet> packet, uint32_t systemId)
{
    g_packetsLost++;
    
    uint32_t senderDeviceId = 0;
    auto it = g_packetToDevice.find(packet->GetUid());
    if (it != g_packetToDevice.end()) {
        senderDeviceId = it->second;
    }
    
    NS_LOG_WARN("[RX-FAIL] GatewayID=" << systemId 
                << " | PacketUID=" << packet->GetUid()
                << " | FromDeviceID=" << senderDeviceId
                << " | Reason=UNDER_SENSITIVITY"
                << " | TotalLost=" << g_packetsLost);
}

/**
 * Callback for no more receivers available
 */
void
OnPhyNoMoreReceivers(Ptr<const Packet> packet, uint32_t systemId)
{
    g_packetsLost++;
    
    uint32_t senderDeviceId = 0;
    auto it = g_packetToDevice.find(packet->GetUid());
    if (it != g_packetToDevice.end()) {
        senderDeviceId = it->second;
    }
    
    NS_LOG_WARN("[RX-FAIL] GatewayID=" << systemId 
                << " | PacketUID=" << packet->GetUid()
                << " | FromDeviceID=" << senderDeviceId
                << " | Reason=NO_RECEIVERS"
                << " | TotalLost=" << g_packetsLost);
}

/**
 * Record a change in the data rate setting on an end device.
 *
 * @param oldDr The previous data rate value.
 * @param newDr The updated data rate value.
 */
void
OnDataRateChange(uint8_t oldDr, uint8_t newDr)
{
    NS_LOG_INFO("[ADR] Data Rate changed: DR" << unsigned(oldDr) << " -> DR" << unsigned(newDr)
                << " (SF" << (12 - oldDr) << " -> SF" << (12 - newDr) << ")");
}

/**
 * Record a change in the transmission power setting on an end device.
 *
 * @param oldTxPower The previous transmission power value.
 * @param newTxPower The updated transmission power value.
 */
void
OnTxPowerChange(double oldTxPower, double newTxPower)
{
    NS_LOG_INFO("[ADR] TxPower changed: " << oldTxPower << " dBm -> " << newTxPower << " dBm");
}

/**
 * Create output directories for results
 */
void CreateOutputDirectories(int scenario)
{
    struct stat st = {0};
    if (stat("resultsfinal", &st) == -1) {
        mkdir("resultsfinal", 0700);
    }
    if (stat("resultsfinal/summaries", &st) == -1) {
        mkdir("resultsfinal/summaries", 0700);
    }

    // Map scenario to folder name
    std::string scenarioName;
    switch (scenario) {
        case 1: scenarioName = "density"; break;
        case 2: scenarioName = "mobilite"; break;
        case 3: scenarioName = "sigma"; break;
        case 4: scenarioName = "intervalle_d_envoie"; break;
        default: scenarioName = "scenario" + std::to_string(scenario); break;
    }

    std::string scenarioDir = "resultsfinal/summaries/" + scenarioName;
    if (stat(scenarioDir.c_str(), &st) == -1) {
        mkdir(scenarioDir.c_str(), 0700);
    }
}

/**
 * Get scenario directory name
 */
std::string GetScenarioName(int scenario)
{
    switch (scenario) {
        case 1: return "density";
        case 2: return "mobilite";
        case 3: return "sigma";
        case 4: return "intervalle_d_envoie";
        default: return "scenario" + std::to_string(scenario);
    }
}

int
main(int argc, char* argv[])
{
    // --- Command line parameters (same as original simulation) ---
    int numDevices = 100;
    double mobilitySpeed = 0.0;           // km/h
    double trafficInterval = 60.0;        // seconds
    double maxRandomLossDb = 10.0;        // dB - replaces sigma for random loss
    std::string adrAlgoStr = "ADR-AVG";   // No-ADR, ADR-MAX, ADR-AVG, ADR-Lite
    int runNumber = 1;
    double simulationTime = 3600.0;       // seconds
    int scenario = 1;
    double radiusMeters = 500.0;          // Deployment radius in meters
    bool initializeSF = true;             // Whether to initialize SFs
    bool enableEnergyModel = true;        // Whether to enable energy tracking
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numDevices", "Number of end devices", numDevices);
    cmd.AddValue("mobilitySpeed", "Mobility speed in km/h", mobilitySpeed);
    cmd.AddValue("trafficInterval", "Traffic interval in seconds", trafficInterval);
    cmd.AddValue("maxRandomLoss", "Maximum random loss in dB (replaces sigma)", maxRandomLossDb);
    cmd.AddValue("adrAlgo", "ADR Algorithm (No-ADR, ADR-MAX, ADR-AVG, ADR-Lite)", adrAlgoStr);
    cmd.AddValue("runNumber", "Run number for repetitions", runNumber);
    cmd.AddValue("scenario", "Scenario number (1=density, 2=mobility, 3=sigma, 4=interval)", scenario);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("radius", "Deployment radius in meters", radiusMeters);
    cmd.AddValue("initializeSF", "Whether to initialize SFs", initializeSF);
    cmd.AddValue("enableEnergyModel", "Whether to enable energy model", enableEnergyModel);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.Parse(argc, argv);

    // Set seed for reproducibility
    RngSeedManager::SetSeed(runNumber);
    RngSeedManager::SetRun(runNumber);

    // Determine ADR mode and settings based on algorithm choice
    // Available algorithms:
    //   No-ADR   : ADR disabled, SF (7-12) and TxPower (2-14 dBm) randomly assigned to each device
    //   ADR-MAX  : Uses maximum SNR from packet history (ns3::AdrComponent)
    //   ADR-AVG  : Uses average SNR from packet history (ns3::AdrComponent) - standard LoRaWAN ADR
    //   ADR-MIN  : Uses minimum SNR from packet history (ns3::AdrComponent) - conservative approach
    //   ADR-Lite : Binary search based ADR without packet history (ns3::AdrLiteComponent)
    
    bool adrEnabled = (adrAlgoStr != "No-ADR");
    std::string snrCombiningMethod = "avg";  // Default: average
    int historyRange = 20;                    // Default: 20 packets
    std::string adrTypeId = "ns3::AdrComponent";  // Default ADR component
    bool useAdrLite = false;
    
    if (adrAlgoStr == "ADR-MAX") {
        snrCombiningMethod = "max";
        historyRange = 20;
        adrTypeId = "ns3::AdrComponent";
    } else if (adrAlgoStr == "ADR-AVG") {
        snrCombiningMethod = "avg";
        historyRange = 20;
        adrTypeId = "ns3::AdrComponent";
    } else if (adrAlgoStr == "ADR-MIN") {
        snrCombiningMethod = "min";
        historyRange = 20;
        adrTypeId = "ns3::AdrComponent";
    } else if (adrAlgoStr == "ADR-Lite") {
        // ADR-Lite uses binary search, no packet history needed
        useAdrLite = true;
        adrTypeId = "ns3::AdrLiteComponent";
        NS_LOG_INFO("Using ADR-Lite algorithm (binary search, no packet history)");
    } else if (adrAlgoStr != "No-ADR") {
        // Default to ADR-AVG for unknown strings
        adrAlgoStr = "ADR-AVG";
        snrCombiningMethod = "avg";
        historyRange = 20;
        adrTypeId = "ns3::AdrComponent";
    }
    
    // Configure ADR attributes globally
    if (adrEnabled && !useAdrLite) {
        // Configure standard AdrComponent parameters
        Config::SetDefault("ns3::AdrComponent::MultiplePacketsCombiningMethod", 
                          StringValue(snrCombiningMethod));
        Config::SetDefault("ns3::AdrComponent::HistoryRange", IntegerValue(historyRange));
        Config::SetDefault("ns3::AdrComponent::ChangeTransmissionPower", BooleanValue(true));
    } else if (adrEnabled && useAdrLite) {
        // Configure AdrLiteComponent parameters
        Config::SetDefault("ns3::AdrLiteComponent::ChangeTransmissionPower", BooleanValue(true));
    }

    // Print simulation parameters
    std::cout << "========================================" << std::endl;
    std::cout << "  LoRaWAN ADR Simulation Parameters" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Devices:          " << numDevices << std::endl;
    std::cout << "  Mobility Speed:   " << mobilitySpeed << " km/h" << std::endl;
    std::cout << "  Traffic Interval: " << trafficInterval << " s" << std::endl;
    std::cout << "  Max Random Loss:  " << maxRandomLossDb << " dB" << std::endl;
    std::cout << "  ADR Algorithm:    " << adrAlgoStr;
    if (adrEnabled && !useAdrLite) {
        std::cout << " (SNR: " << snrCombiningMethod << ", History: " << historyRange << ")";
    } else if (useAdrLite) {
        std::cout << " (Binary Search, No History)";
    }
    std::cout << std::endl;
    std::cout << "  ADR Component:    " << adrTypeId << std::endl;
    std::cout << "  Simulation Time:  " << simulationTime << " s" << std::endl;
    std::cout << "  Scenario:         " << scenario << " (" << GetScenarioName(scenario) << ")" << std::endl;
    std::cout << "  Run Number:       " << runNumber << std::endl;
    std::cout << "  Radius:           " << radiusMeters << " m" << std::endl;
    std::cout << "  Energy Model:     " << (enableEnergyModel ? "Enabled" : "Disabled") << std::endl;
    std::cout << "========================================" << std::endl;

    NS_LOG_INFO("Random seed set to run number: " << runNumber);
    NS_LOG_INFO("ADR Algorithm: " << adrAlgoStr << (adrEnabled ? " (enabled)" : " (disabled)"));
    
    // Calculate mobile node probability based on mobility speed
    double mobileNodeProbability = (mobilitySpeed > 0.1) ? 1.0 : 0.0;
    
    // Convert mobility speed from km/h to m/s
    double minSpeedMps = 0.5;
    double maxSpeedMps = mobilitySpeed / 3.6;
    if (maxSpeedMps < minSpeedMps) {
        maxSpeedMps = minSpeedMps;
    }

    // Create output directories
    CreateOutputDirectories(scenario);

    // --- Logging Setup ---
    LogComponentEnable("LoRaWANADRSimulationModule", LOG_LEVEL_ALL);
    if (verbose) {
        LogComponentEnable("AdrComponent", LOG_LEVEL_ALL);
        LogComponentEnable("LoraChannel", LOG_LEVEL_INFO);
        LogComponentEnable("EndDeviceLoraPhy", LOG_LEVEL_ALL);
        LogComponentEnable("GatewayLoraPhy", LOG_LEVEL_ALL);
        LogComponentEnable("EndDeviceLorawanMac", LOG_LEVEL_ALL);
        LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_ALL);
        LogComponentEnable("NetworkServer", LOG_LEVEL_ALL);
        LogComponentEnable("NetworkController", LOG_LEVEL_ALL);
        LogComponentEnable("LoraInterferenceHelper", LOG_LEVEL_ALL);
    }
    LogComponentEnableAll(LOG_PREFIX_FUNC);
    LogComponentEnableAll(LOG_PREFIX_NODE);
    LogComponentEnableAll(LOG_PREFIX_TIME);
    
    NS_LOG_INFO("Logging initialized. Verbose mode: " << (verbose ? "ON" : "OFF"));

    // Set the end devices to allow data rate control (ADR) from the network server
    Config::SetDefault("ns3::EndDeviceLorawanMac::ADR", BooleanValue(adrEnabled));

    /************************
     *  Create the channel  *
     ************************/

    NS_LOG_INFO("Creating the channel...");

    // Create the lora channel object with log-distance propagation loss
    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetPathLossExponent(3.76);
    loss->SetReference(1, 7.7);

    // Add random loss component (similar to sigma in original simulation)
    if (maxRandomLossDb > 0) {
        Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
        x->SetAttribute("Min", DoubleValue(0.0));
        x->SetAttribute("Max", DoubleValue(maxRandomLossDb));

        Ptr<RandomPropagationLossModel> randomLoss = CreateObject<RandomPropagationLossModel>();
        randomLoss->SetAttribute("Variable", PointerValue(x));

        loss->SetNext(randomLoss);
    }

    Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel>();

    Ptr<LoraChannel> channel = CreateObject<LoraChannel>(loss, delay);
    
    NS_LOG_INFO("Channel created with path loss exponent 3.76 and random loss up to " 
                << maxRandomLossDb << " dB");

    /************************
     *  Create the helpers  *
     ************************/

    NS_LOG_INFO("Setting up helpers...");

    // End device mobility
    MobilityHelper mobilityEd;
    MobilityHelper mobilityGw;
    
    // Position allocator for end devices (similar to original: random in box)
    mobilityEd.SetPositionAllocator(
        "ns3::RandomRectanglePositionAllocator",
        "X",
        PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
            "Min", DoubleValue(-radiusMeters),
            "Max", DoubleValue(radiusMeters))),
        "Y",
        PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
            "Min", DoubleValue(-radiusMeters),
            "Max", DoubleValue(radiusMeters))));

    // Gateway position (center at height 15m)
    Ptr<ListPositionAllocator> positionAllocGw = CreateObject<ListPositionAllocator>();
    positionAllocGw->Add(Vector(0.0, 0.0, 15.0));
    mobilityGw.SetPositionAllocator(positionAllocGw);
    mobilityGw.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Create the LoraPhyHelper
    LoraPhyHelper phyHelper = LoraPhyHelper();
    phyHelper.SetChannel(channel);

    // Create the LorawanMacHelper
    LorawanMacHelper macHelper = LorawanMacHelper();

    // Create the LoraHelper
    LoraHelper helper = LoraHelper();
    helper.EnablePacketTracking();

    /*********************
     *  Create Gateways  *
     *********************/

    NS_LOG_INFO("Creating gateway...");
    
    NodeContainer gateways;
    gateways.Create(1);
    mobilityGw.Install(gateways);

    // Create the LoraNetDevices of the gateways
    phyHelper.SetDeviceType(LoraPhyHelper::GW);
    macHelper.SetDeviceType(LorawanMacHelper::GW);
    helper.Install(phyHelper, macHelper, gateways);
    
    NS_LOG_INFO("Gateway created at position (0, 0, 15m)");

    /************************
     *  Create End Devices  *
     ************************/

    NS_LOG_INFO("Creating " << numDevices << " end devices...");

    NodeContainer endDevices;
    endDevices.Create(numDevices);

    // Calculate number of fixed vs mobile nodes
    int fixedPositionNodes = static_cast<int>(numDevices * (1.0 - mobileNodeProbability));
    
    // Install mobility model on fixed nodes
    mobilityEd.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for (int i = 0; i < fixedPositionNodes; ++i) {
        mobilityEd.Install(endDevices.Get(i));
    }
    
    // Install mobility model on mobile nodes
    if (mobileNodeProbability > 0 && fixedPositionNodes < numDevices) {
        mobilityEd.SetMobilityModel(
            "ns3::RandomWalk2dMobilityModel",
            "Bounds",
            RectangleValue(Rectangle(-radiusMeters * 2, radiusMeters * 2, 
                                     -radiusMeters * 2, radiusMeters * 2)),
            "Distance",
            DoubleValue(1000),
            "Speed",
            PointerValue(CreateObjectWithAttributes<UniformRandomVariable>(
                "Min", DoubleValue(minSpeedMps),
                "Max", DoubleValue(maxSpeedMps))));
        
        for (int i = fixedPositionNodes; i < numDevices; ++i) {
            mobilityEd.Install(endDevices.Get(i));
        }
    }

    // Set end device height to 1.5m
    for (auto j = endDevices.Begin(); j != endDevices.End(); ++j) {
        Ptr<MobilityModel> mobility = (*j)->GetObject<MobilityModel>();
        Vector position = mobility->GetPosition();
        position.z = 1.5;
        mobility->SetPosition(position);
    }
    
    NS_LOG_INFO("End devices created: " << fixedPositionNodes << " fixed, " 
                << (numDevices - fixedPositionNodes) << " mobile");

    // Store to global containers for trace callbacks
    g_endDevices = endDevices;
    g_gateways = gateways;
    
    // Log detailed device information
    LogDeviceDetails(endDevices, fixedPositionNodes);

    // Create a LoraDeviceAddressGenerator
    uint8_t nwkId = 54;
    uint32_t nwkAddr = 1864;
    Ptr<LoraDeviceAddressGenerator> addrGen =
        CreateObject<LoraDeviceAddressGenerator>(nwkId, nwkAddr);

    // Create the LoraNetDevices of the end devices
    phyHelper.SetDeviceType(LoraPhyHelper::ED);
    macHelper.SetDeviceType(LorawanMacHelper::ED_A);
    macHelper.SetAddressGenerator(addrGen);
    macHelper.SetRegion(LorawanMacHelper::EU);
    NetDeviceContainer endDevicesNetDevices = helper.Install(phyHelper, macHelper, endDevices);

    // Initialize spreading factors and transmission power
    if (initializeSF) {
        if (!adrEnabled) {
            // No-ADR: Random assignment of SF and TxPower for each device
            NS_LOG_INFO("No-ADR mode: Randomly assigning SF and TxPower to each device...");
            
            Ptr<UniformRandomVariable> sfRandom = CreateObject<UniformRandomVariable>();
            sfRandom->SetAttribute("Min", DoubleValue(7.0));
            sfRandom->SetAttribute("Max", DoubleValue(12.99));  // Will be truncated to 7-12
            
            Ptr<UniformRandomVariable> txPowerRandom = CreateObject<UniformRandomVariable>();
            txPowerRandom->SetAttribute("Min", DoubleValue(0.0));
            txPowerRandom->SetAttribute("Max", DoubleValue(6.99));  // TxPower index 0-6
            
            // EU868 TxPower levels: index 0=14dBm, 1=12dBm, 2=10dBm, 3=8dBm, 4=6dBm, 5=4dBm, 6=2dBm
            std::vector<double> txPowerLevels = {14.0, 12.0, 10.0, 8.0, 6.0, 4.0, 2.0};
            std::vector<int> sfDistribution(6, 0);  // Count SF7-SF12
            std::map<int, int> txPowerDistribution;
            
            for (auto j = endDevices.Begin(); j != endDevices.End(); ++j) {
                Ptr<Node> node = *j;
                Ptr<LoraNetDevice> loraDevice = node->GetDevice(0)->GetObject<LoraNetDevice>();
                Ptr<ClassAEndDeviceLorawanMac> mac = loraDevice->GetMac()->GetObject<ClassAEndDeviceLorawanMac>();
                
                // Random SF (7-12)
                uint8_t randomSF = static_cast<uint8_t>(sfRandom->GetValue());
                if (randomSF > 12) randomSF = 12;
                if (randomSF < 7) randomSF = 7;
                
                // Random TxPower index (0-6)
                uint8_t txPowerIndex = static_cast<uint8_t>(txPowerRandom->GetValue());
                if (txPowerIndex > 6) txPowerIndex = 6;
                double randomTxPower = txPowerLevels[txPowerIndex];
                
                // Set the parameters
                mac->SetDataRate(12 - randomSF);  // DR = 12 - SF (EU868: SF7=DR5, SF12=DR0)
                mac->SetTransmissionPowerDbm(randomTxPower);
                
                // Update distribution counters
                sfDistribution[randomSF - 7]++;
                txPowerDistribution[static_cast<int>(randomTxPower)]++;
                
                NS_LOG_DEBUG("[No-ADR] Device " << node->GetId() 
                            << " -> SF" << (int)randomSF 
                            << ", TxPower=" << randomTxPower << " dBm");
            }
            
            NS_LOG_INFO("Random SF Distribution:");
            NS_LOG_INFO("  [0] SF7:  " << sfDistribution[0]);
            NS_LOG_INFO("  [1] SF8:  " << sfDistribution[1]);
            NS_LOG_INFO("  [2] SF9:  " << sfDistribution[2]);
            NS_LOG_INFO("  [3] SF10: " << sfDistribution[3]);
            NS_LOG_INFO("  [4] SF11: " << sfDistribution[4]);
            NS_LOG_INFO("  [5] SF12: " << sfDistribution[5]);
            
            NS_LOG_INFO("Random TxPower Distribution:");
            for (auto& kv : txPowerDistribution) {
                NS_LOG_INFO("  TxPower " << kv.first << " dBm: " << kv.second << " devices");
            }
        } else {
            // ADR enabled: Initialize SF based on distance to gateway
            NS_LOG_INFO("ADR mode: Initializing spreading factors based on distance to gateway...");
            std::vector<int> sfDistribution = LorawanMacHelper::SetSpreadingFactorsUp(endDevices, gateways, channel);
            NS_LOG_INFO("SF Distribution:");
            NS_LOG_INFO("  [0] SF7:  " << sfDistribution[0]);
            NS_LOG_INFO("  [1] SF8:  " << sfDistribution[1]);
            NS_LOG_INFO("  [2] SF9:  " << sfDistribution[2]);
            NS_LOG_INFO("  [3] SF10: " << sfDistribution[3]);
            NS_LOG_INFO("  [4] SF11: " << sfDistribution[4]);
            NS_LOG_INFO("  [5] SF12: " << sfDistribution[5]);
        }
    }

    /*********************************************
     *  Install applications on the end devices  *
     *********************************************/

    NS_LOG_INFO("Installing applications with period " << trafficInterval << " seconds...");

    PeriodicSenderHelper appHelper = PeriodicSenderHelper();
    appHelper.SetPeriod(Seconds(trafficInterval));
    appHelper.SetPacketSize(50);  // 50 bytes payload as in original

    ApplicationContainer appContainer = appHelper.Install(endDevices);
    appContainer.Start(Seconds(0));
    appContainer.Stop(Seconds(simulationTime));

    /************************
     * Install Energy Model *
     ************************/

    EnergySourceContainer sources;
    DeviceEnergyModelContainer deviceModels;

    if (enableEnergyModel) {
        NS_LOG_INFO("Installing energy model...");

        BasicEnergySourceHelper basicSourceHelper;
        LoraRadioEnergyModelHelper radioEnergyHelper;

        // Configure energy source
        basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10000)); // 10000 J
        basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.3));

        // Configure radio energy model (typical LoRa values)
        radioEnergyHelper.Set("StandbyCurrentA", DoubleValue(0.0014));
        radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.028));
        radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
        radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0112));

        radioEnergyHelper.SetTxCurrentModel("ns3::ConstantLoraTxCurrentModel",
                                            "TxCurrent", DoubleValue(0.028));

        // Install source on end devices' nodes
        sources = basicSourceHelper.Install(endDevices);

        // Install device model
        deviceModels = radioEnergyHelper.Install(endDevicesNetDevices, sources);

        // Store initial energy for later calculation
        for (uint32_t i = 0; i < endDevices.GetN(); ++i) {
            Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(sources.Get(i));
            if (source) {
                g_initialEnergy[i] = source->GetInitialEnergy();
                g_energySources[i] = source;
            }
        }
    }

    /**************************
     *  Create network server  *
     ***************************/

    NS_LOG_INFO("Creating network server...");

    Ptr<Node> networkServer = CreateObject<Node>();

    // PointToPoint links between gateways and server
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    
    // Store network server app registration details
    P2PGwRegistration_t gwRegistration;
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw) {
        auto container = p2p.Install(networkServer, *gw);
        auto serverP2PNetDev = DynamicCast<PointToPointNetDevice>(container.Get(0));
        gwRegistration.emplace_back(serverP2PNetDev, *gw);
    }

    // Install the NetworkServer application on the network server
    NetworkServerHelper networkServerHelper;
    networkServerHelper.EnableAdr(adrEnabled);
    networkServerHelper.SetAdr(adrTypeId);  // Use selected ADR component (AdrComponent or AdrLiteComponent)
    networkServerHelper.SetGatewaysP2P(gwRegistration);
    networkServerHelper.SetEndDevices(endDevices);
    networkServerHelper.Install(networkServer);
    
    NS_LOG_INFO("ADR component type: " << adrTypeId);

    // Install the Forwarder application on the gateways
    ForwarderHelper forwarderHelper;
    forwarderHelper.Install(gateways);
    
    NS_LOG_INFO("Network server installed with ADR " << (adrEnabled ? "enabled" : "disabled"));

    // Connect traces for monitoring PHY events
    NS_LOG_INFO("Connecting trace sources for packet tracking...");
    
    // Connect traces for ADR monitoring
    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/0/$ns3::LoraNetDevice/Mac/$ns3::EndDeviceLorawanMac/TxPower",
        MakeCallback(&OnTxPowerChange));
    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/0/$ns3::LoraNetDevice/Mac/$ns3::EndDeviceLorawanMac/DataRate",
        MakeCallback(&OnDataRateChange));
    
    // Connect to MAC layer traces for packet monitoring - End Devices TX
    for (auto j = endDevices.Begin(); j != endDevices.End(); ++j) {
        Ptr<Node> node = *j;
        uint32_t nodeId = node->GetId();
        Ptr<LoraNetDevice> loraNetDevice = DynamicCast<LoraNetDevice>(node->GetDevice(0));
        if (loraNetDevice) {
            Ptr<LorawanMac> mac = loraNetDevice->GetMac();
            if (mac) {
                // Connect to SentNewPacket trace
                mac->TraceConnectWithoutContext(
                    "SentNewPacket",
                    MakeBoundCallback(&OnEndDeviceSend, nodeId));
            }
        }
    }
    
    // Connect to Gateway MAC layer traces for RX monitoring
    for (auto gw = gateways.Begin(); gw != gateways.End(); ++gw) {
        Ptr<Node> gwNode = *gw;
        uint32_t gwNodeId = gwNode->GetId();
        Ptr<LoraNetDevice> gwLoraNetDevice = DynamicCast<LoraNetDevice>(gwNode->GetDevice(0));
        if (gwLoraNetDevice) {
            Ptr<LorawanMac> gwMac = gwLoraNetDevice->GetMac();
            if (gwMac) {
                // Connect to ReceivedPacket trace on gateway MAC
                gwMac->TraceConnectWithoutContext(
                    "ReceivedPacket",
                    MakeBoundCallback(&OnGatewayReceive, gwNodeId));
            }
        }
    }
    
    NS_LOG_INFO("Trace sources connected for " << endDevices.GetN() << " end devices and " 
                << gateways.GetN() << " gateways");

    // Enable periodic status printing (optional)
    Time stateSamplePeriod = Seconds(trafficInterval * 10);
    std::string nodeDataFilename = "resultsfinal/nodeData_run" + std::to_string(runNumber) + ".txt";
    helper.EnablePeriodicDeviceStatusPrinting(endDevices, gateways, nodeDataFilename, stateSamplePeriod);

    /****************
     *  Simulation  *
     ****************/

    NS_LOG_INFO("Starting simulation for " << simulationTime << " seconds...");
    std::cout << "\n[SIMULATION STARTING]" << std::endl;
    
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    
    std::cout << "\n[SIMULATION COMPLETED]" << std::endl;

    /***************************
     *  Collect and save results  *
     ***************************/

    NS_LOG_INFO("Computing performance metrics...");

    LoraPacketTracker& tracker = helper.GetPacketTracker();
    
    // Get packet statistics
    std::string macPacketStats = tracker.CountMacPacketsGlobally(Seconds(0), Seconds(simulationTime));
    
    // Parse the stats (format: "sent received" as doubles)
    std::istringstream iss(macPacketStats);
    double totalPacketsD = 0, successfulPacketsD = 0;
    iss >> totalPacketsD >> successfulPacketsD;
    uint32_t totalPackets = static_cast<uint32_t>(totalPacketsD);
    uint32_t successfulPackets = static_cast<uint32_t>(successfulPacketsD);

    double pdr = (totalPackets == 0) ? 0.0 : (static_cast<double>(successfulPackets) / totalPackets) * 100.0;

    // Calculate total energy consumption
    double totalEnergyConsumption = 0.0;
    if (enableEnergyModel) {
        for (uint32_t i = 0; i < endDevices.GetN(); ++i) {
            if (g_energySources.find(i) != g_energySources.end()) {
                double initialEnergy = g_initialEnergy[i];
                double remainingEnergy = g_energySources[i]->GetRemainingEnergy();
                totalEnergyConsumption += (initialEnergy - remainingEnergy);
            }
        }
    }

    double avgEnergyPerPacket_mJ = (successfulPackets == 0) ? 0.0 : 
                                   (totalEnergyConsumption / successfulPackets) * 1000.0;

    // Log performance summary
    NS_LOG_INFO("========== SIMULATION RESULTS ==========");
    NS_LOG_INFO("Total packets sent:     " << totalPackets);
    NS_LOG_INFO("Successful packets:     " << successfulPackets);
    NS_LOG_INFO("Packet Delivery Ratio:  " << pdr << " %");
    NS_LOG_INFO("Total Energy consumed:  " << totalEnergyConsumption << " J");
    NS_LOG_INFO("Avg Energy per packet:  " << avgEnergyPerPacket_mJ << " mJ");
    NS_LOG_INFO("========================================");

    // --- Save detailed results ---
    std::ostringstream filename;
    filename << "resultsfinal/sim_scen" << scenario
             << "_dev" << numDevices 
             << "_mob" << std::fixed << std::setprecision(1) << mobilitySpeed
             << "_traf" << static_cast<int>(trafficInterval)
             << "_sig" << std::setprecision(2) << maxRandomLossDb
             << "_" << adrAlgoStr
             << "_run" << runNumber << ".csv";

    std::ofstream outputFile(filename.str());
    outputFile << "Scenario,NumDevices,MobilitySpeed,TrafficInterval,MaxRandomLoss,ADR,RunNumber,"
               << "TotalPackets,SuccessfulPackets,PDR_Percent,TotalEnergy_J,AvgEnergy_mJ\n";
    outputFile << scenario << ","
               << numDevices << ","
               << std::fixed << std::setprecision(1) << mobilitySpeed << ","
               << static_cast<int>(trafficInterval) << ","
               << std::setprecision(2) << maxRandomLossDb << ","
               << adrAlgoStr << ","
               << runNumber << ","
               << totalPackets << ","
               << successfulPackets << ","
               << std::setprecision(2) << pdr << ","
               << std::setprecision(6) << totalEnergyConsumption << ","
               << std::setprecision(6) << avgEnergyPerPacket_mJ << "\n";
    outputFile.close();

    // --- Save summary in scenario folder ---
    std::string scenarioName = GetScenarioName(scenario);
    std::ostringstream summaryFilename;
    summaryFilename << "resultsfinal/summaries/" << scenarioName << "/summary_scen" << scenario
                    << "_dev" << numDevices 
                    << "_mob" << std::fixed << std::setprecision(1) << mobilitySpeed
                    << "_traf" << static_cast<int>(trafficInterval)
                    << "_sig" << std::setprecision(2) << maxRandomLossDb
                    << "_" << adrAlgoStr
                    << "_run" << runNumber << ".csv";

    std::ofstream summaryFile(summaryFilename.str());
    summaryFile << "NumDevices,MobilitySpeed,TrafficInterval,MaxRandomLoss,RunNumber,"
                << "TotalPackets,SuccessfulPackets,PDR_Percent,AvgEnergy_mJ\n";
    summaryFile << numDevices << ","
                << std::fixed << std::setprecision(1) << mobilitySpeed << ","
                << static_cast<int>(trafficInterval) << ","
                << std::setprecision(2) << maxRandomLossDb << ","
                << runNumber << ","
                << totalPackets << ","
                << successfulPackets << ","
                << std::setprecision(2) << pdr << ","
                << std::setprecision(6) << avgEnergyPerPacket_mJ << "\n";
    summaryFile.close();

    // Print summary to console
    std::cout << "Run " << runNumber << " (" << adrAlgoStr << "): "
              << "PDR=" << std::fixed << std::setprecision(2) << pdr << "%, "
              << "Energy=" << std::setprecision(6) << avgEnergyPerPacket_mJ << " mJ, "
              << "Packets=" << totalPackets << " (sent), " 
              << successfulPackets << " (received)" << std::endl;

    NS_LOG_INFO("MAC packets: " << macPacketStats);
    NS_LOG_INFO("Results saved to: " << filename.str());
    NS_LOG_INFO("Summary saved to: " << summaryFilename.str());

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Simulation Complete!" << std::endl;
    std::cout << "========================================" << std::endl;

    // Clear all global maps and containers before Simulator::Destroy to avoid dangling pointers
    g_energySources.clear();
    g_initialEnergy.clear();
    g_packetToDevice.clear();
    g_deviceMessageCount.clear();
    g_endDevices = NodeContainer();  // Reset to empty container
    g_gateways = NodeContainer();    // Reset to empty container
    g_packetsSent = 0;
    g_packetsReceived = 0;
    g_packetsLost = 0;
    
    // Note: SIGSEGV may occur during Simulator::Destroy() due to ns-3 internal cleanup
    // This is a known issue with some ns-3 modules and does not affect simulation results
    Simulator::Destroy();

    return 0;
}
