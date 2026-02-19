#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/random-variable-stream.h"
#include "ns3/command-line.h"
#include "ns3/vector.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/random-walk-2d-mobility-model.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <numeric>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("lorawan-adr-simulationfinal");

// --- Structures de données et Énumérations ---

enum class ADRAlgorithm {
    NO_ADR,
    ADR_MAX,
    ADR_AVG,
    ADR_LITE
};

std::string ADRAlgorithmToString(ADRAlgorithm algo) {
    switch (algo) {
        case ADRAlgorithm::NO_ADR:   return "No-ADR";
        case ADRAlgorithm::ADR_MAX:  return "ADR-MAX";
        case ADRAlgorithm::ADR_AVG:  return "ADR-AVG";
        case ADRAlgorithm::ADR_LITE: return "ADR-Lite";
        default: return "Unknown";
    }
}

struct LoRaConfiguration {
    int k_index = 0;
    int sf = 7;
    double txPower = 14.0;
    int cr = 1;
    double cf = 868.1;
    int bw = 125;
    double energyConsumption = 0.0;

    void CalculateEnergy(int payloadSize) {
        double bw_hz = bw * 1000.0;
        double t_sym = std::pow(2, sf) / bw_hz;
        double h = 0, de = 0;
        
        double theta = 8.0 * payloadSize - 4.0 * sf + 28.0 + 16.0 - 20.0 * h;
        double gamma = 4.0 * (sf - 2.0 * de);
        double n_payload = 8.0 + std::max(0.0, std::ceil(theta / gamma) * (cr + 4.0));
        
        double t_preamble = (8.0 + 4.25) * t_sym;
        double t_payload = n_payload * t_sym;
        double time_on_air = t_preamble + t_payload;
        
        double tx_power_watts = std::pow(10, (txPower - 30.0) / 10.0);
        energyConsumption = tx_power_watts * time_on_air;
    }

    bool operator<(const LoRaConfiguration& other) const {
        return energyConsumption < other.energyConsumption;
    }
};

struct ADRLiteDeviceState {
    uint32_t current_k = 0;
    bool initialized = false;
};

struct NoADRDeviceState {
    double packetInterval = 50.0;
    bool initialized = false;
};

struct SNRHistory {
    std::vector<double> snrValues;
    void Add(double snr) {
        snrValues.push_back(snr);
        if (snrValues.size() > 20) snrValues.erase(snrValues.begin());
    }
    double GetMax() const { 
        return snrValues.empty() ? -99 : *std::max_element(snrValues.begin(), snrValues.end()); 
    }
    double GetAvg() const { 
        return snrValues.empty() ? -99 : std::accumulate(snrValues.begin(), snrValues.end(), 0.0) / snrValues.size(); 
    }
    size_t size() const { return snrValues.size(); }
};

struct PacketLog {
    uint32_t deviceId;
    double time;
    bool success;
    double rssi, snr;
    LoRaConfiguration config;
};

struct OngoingTransmission {
    uint32_t deviceId;
    double startTime;
    double endTime;
    LoRaConfiguration config;
    double rssi, snr;
    Vector position;
};

// --- Classe Gateway (Network Server) ---

class LoRaGateway : public Object {
public:

    LoRaGateway() {
        m_position = Vector(0, 0, 15);
        m_sfSensitivity = {{7, -124}, {8, -127}, {9, -130}, {10, -133}, {11, -135}, {12, -137}};
        m_sfMinSNR = {{7, -7.5}, {8, -10}, {9, -12.5}, {10, -15}, {11, -17.5}, {12, -20}};
        m_shadowingRng = CreateObject<NormalRandomVariable>();
        m_randomVar = CreateObject<UniformRandomVariable>();
        m_currentTransmissions.clear();
    }

    void SetPosition(Vector pos) { m_position = pos; }
    
    void SetChannelSaturation(double sigma) {
        m_shadowingRng->SetAttribute("Mean", DoubleValue(0.0));
        m_shadowingRng->SetAttribute("Variance", DoubleValue(sigma * sigma));
    }

    void InitializeConfigurations(int payloadSize, int config_type) {
        m_configurations.clear();
        
        std::vector<int> sfs = {7, 8, 9, 10, 11, 12};
        std::vector<double> tps = {2, 5, 8, 11, 14};
        std::vector<int> crs = {1};
        std::vector<double> cfs = {868.1};
        
        if (config_type == 2 || config_type == 4) {
            cfs = {868.1, 868.4, 868.7};
        }
        if (config_type == 3 || config_type == 4) {
            crs = {1, 4};
        }

        for (int sf : sfs) {
            for (double tp : tps) {
                for (int cr : crs) {
                    for (double cf : cfs) {
                        LoRaConfiguration config;
                        config.sf = sf; 
                        config.txPower = tp; 
                        config.cr = cr; 
                        config.cf = cf; 
                        config.bw = 125;
                        config.CalculateEnergy(payloadSize);
                        m_configurations.push_back(config);
                    }
                }
            }
        }

        std::sort(m_configurations.begin(), m_configurations.end());
        
        for (size_t i = 0; i < m_configurations.size(); ++i) {
            m_configurations[i].k_index = i + 1;
        }
    }

    bool ReceivePacket(const Vector& pos, const LoRaConfiguration& cfg, double& rssi, double& snr, uint32_t deviceId, bool isMobile = false) {
        double distance_m = CalculateDistance(m_position, pos);
        if (distance_m < 1.0) distance_m = 1.0;
        double distance_km = distance_m / 1000.0;

        double pathLoss = 69.55 + 26.16 * std::log10(868.0) - 13.82 * std::log10(15.0) + 
                         (44.9 - 6.55 * std::log10(15.0)) * std::log10(distance_km);
        
        pathLoss += m_shadowingRng->GetValue();
        
        if (isMobile) {
            Ptr<ExponentialRandomVariable> rayleighRng = CreateObject<ExponentialRandomVariable>();
            rayleighRng->SetAttribute("Mean", DoubleValue(0.8));
            double rayleighFading = -10.0 * std::log10(rayleighRng->GetValue());
            
            Ptr<UniformRandomVariable> mobilePenaltyRng = CreateObject<UniformRandomVariable>();
            mobilePenaltyRng->SetAttribute("Min", DoubleValue(5.0));
            mobilePenaltyRng->SetAttribute("Max", DoubleValue(15.0));
            double mobilePenalty = mobilePenaltyRng->GetValue();
            
            Ptr<UniformRandomVariable> temporalRng = CreateObject<UniformRandomVariable>();
            temporalRng->SetAttribute("Min", DoubleValue(0.0));
            temporalRng->SetAttribute("Max", DoubleValue(5.0));
            double temporalFading = temporalRng->GetValue();
            
            pathLoss += rayleighFading + mobilePenalty + temporalFading;
        }
        
        rssi = cfg.txPower - pathLoss;
        double noisePower = -174.0 + 10.0 * std::log10(cfg.bw * 1000.0);
        snr = rssi - noisePower;
        
        bool basicReception = (rssi > m_sfSensitivity[cfg.sf]) && (snr > m_sfMinSNR[cfg.sf]);
        
        if (!basicReception) {
            return false;
        }
        
        double currentTime = Simulator::Now().GetSeconds();
        double transmissionTime = CalculateTransmissionTime(cfg, 20);
        double endTime = currentTime + transmissionTime;
        
        auto it = m_currentTransmissions.begin();
        while (it != m_currentTransmissions.end()) {
            if (it->endTime < currentTime) {
                it = m_currentTransmissions.erase(it);
            } else {
                ++it;
            }
        }
        
        bool collision = false;
        for (const auto& ongoing : m_currentTransmissions) {
            if (ongoing.config.sf == cfg.sf) {
                double powerDifference = std::abs(ongoing.rssi - rssi);
                
                double captureThreshold;
                switch (cfg.sf) {
                    case 7:  captureThreshold = 10.0; break;
                    case 8:  captureThreshold = 9.0;  break;
                    case 9:  captureThreshold = 8.0;  break;
                    case 10: captureThreshold = 7.0;  break;
                    case 11: captureThreshold = 6.0;  break;
                    case 12: captureThreshold = 5.0;  break;
                    default: captureThreshold = 6.0;  break;
                }
                
                if (powerDifference < captureThreshold) {
                    collision = true;
                    break;
                }
            }
        }
        
        OngoingTransmission newTx;
        newTx.deviceId = deviceId;
        newTx.startTime = currentTime;
        newTx.endTime = endTime;
        newTx.config = cfg;
        newTx.rssi = rssi;
        newTx.snr = snr;
        newTx.position = pos;
        m_currentTransmissions.push_back(newTx);
        
        return !collision;
    }

    void UpdateDeviceState(uint32_t id, bool success, double snr, const LoRaConfiguration& lastConfig, bool isMobile = false) {
        if (success) {
            m_snrHistories[id].Add(snr);
        }
        
        m_deviceMobility[id] = isMobile;
        
        auto& liteState = m_adrLiteStates[id];
        
        if (!liteState.initialized) {
            if (isMobile) {
                liteState.current_k = m_configurations.size() * 3 / 4;
            } else {
                liteState.current_k = m_configurations.size() / 2;
            }
            liteState.initialized = true;
        } else {
            uint32_t old_k = liteState.current_k;
            
            if (success) { 
                uint32_t stepSize = isMobile ? 1 : 2;
                
                if (snr > (isMobile ? -3.0 : -5.0) && liteState.current_k > stepSize) {
                    liteState.current_k = std::max(1u, liteState.current_k - stepSize);
                } else if (liteState.current_k > 1) {
                    liteState.current_k = std::max(1u, liteState.current_k - 1);
                }
            } else { 
                uint32_t stepSize = isMobile ? 2 : 1;
                
                if (liteState.current_k < m_configurations.size()) {
                    liteState.current_k = std::min((uint32_t)m_configurations.size(), 
                                                 liteState.current_k + stepSize);
                }
            }
        }
    }

    LoRaConfiguration GetDeviceConfiguration(uint32_t id, ADRAlgorithm algo) {
        auto& state = m_adrLiteStates[id];
        
        if (!state.initialized) {
            bool isMobile = m_deviceMobility[id];
            if (isMobile) {
                state.current_k = m_configurations.size() * 3 / 4;
            } else {
                state.current_k = m_configurations.size() / 2;
            }
            state.initialized = true;
        }
        
        if (algo == ADRAlgorithm::ADR_LITE) {
            return m_configurations[state.current_k - 1];
        }
        
        if (algo == ADRAlgorithm::NO_ADR) {
            if (!m_globalNoAdrConfigInitialized) {
                if (!m_randomVar) {
                    m_randomVar = CreateObject<UniformRandomVariable>();
                }
                
                m_randomVar->SetAttribute("Min", DoubleValue(0.0));
                m_randomVar->SetAttribute("Max", DoubleValue(1.0));
                double randomValue = m_randomVar->GetValue();
                
                size_t selectedIndex;
                if (randomValue < 0.95) {
                    size_t maxLowIndex = std::max(1UL, (m_configurations.size() * 3) / 10);
                    m_randomVar->SetAttribute("Min", DoubleValue(0));
                    m_randomVar->SetAttribute("Max", DoubleValue(maxLowIndex - 1));
                    selectedIndex = static_cast<size_t>(m_randomVar->GetValue());
                } else {
                    m_randomVar->SetAttribute("Min", DoubleValue(0));
                    m_randomVar->SetAttribute("Max", DoubleValue(m_configurations.size() - 1));
                    selectedIndex = static_cast<size_t>(m_randomVar->GetValue());
                }
                
                m_globalNoAdrConfig = m_configurations[selectedIndex];
                m_globalNoAdrConfigInitialized = true;
            }
            
            if (m_noAdrStates.find(id) == m_noAdrStates.end() || !m_noAdrStates[id].initialized) {
                Ptr<ExponentialRandomVariable> expRng = CreateObject<ExponentialRandomVariable>();
                expRng->SetAttribute("Mean", DoubleValue(30.0));
                double fixedInterval = std::max(10.0, std::min(120.0, expRng->GetValue()));
                
                m_noAdrStates[id].packetInterval = fixedInterval;
                m_noAdrStates[id].initialized = true;
            }
            
            return m_globalNoAdrConfig;
        }
        
        bool isMobile = m_deviceMobility[id];
        
        if (m_snrHistories[id].size() >= (isMobile ? 8 : 5)) {
            double targetSINR;
            double snrMargin;
            
            if (algo == ADRAlgorithm::ADR_MAX) {
                targetSINR = m_snrHistories[id].GetMax();
                snrMargin = isMobile ? 8.0 : 4.0;
            } else {
                targetSINR = m_snrHistories[id].GetAvg();
                snrMargin = isMobile ? 6.0 : 3.0;
            }
            
            double effectiveSINR = targetSINR - snrMargin;
            
            for (size_t i = 0; i < m_configurations.size(); ++i) {
                const auto& cfg = m_configurations[i];
                double requiredSINR = m_sfMinSNR[cfg.sf];
                
                if (effectiveSINR >= requiredSINR + (isMobile ? 2.0 : 1.0)) {
                    return cfg; 
                }
            }
        }
        
        size_t adjustedIndex;
        size_t baseIndex = static_cast<size_t>(state.current_k - 1);
        
        if (algo == ADRAlgorithm::ADR_MAX) {
            if (isMobile) {
                adjustedIndex = m_configurations.size() * 3 / 4 + 
                               (baseIndex % (m_configurations.size() / 4));
            } else {
                adjustedIndex = std::min(static_cast<size_t>(m_configurations.size() * 3 / 4), baseIndex);
            }
        } else {
            if (isMobile) {
                adjustedIndex = m_configurations.size() / 2 + 
                               (baseIndex % (m_configurations.size() / 2));
            } else {
                adjustedIndex = m_configurations.size() / 8 + 
                               (baseIndex % (m_configurations.size() * 3 / 4));
            }
        }
        
        return m_configurations[adjustedIndex];
    }
    
    double GetDevicePacketInterval(uint32_t id, ADRAlgorithm algo) {
        if (algo == ADRAlgorithm::NO_ADR) {
            GetDeviceConfiguration(id, algo);
            if (m_noAdrStates.find(id) != m_noAdrStates.end()) {
                return m_noAdrStates[id].packetInterval;
            }
        }
        return 50.0;
    }
    
private:
    double CalculateTransmissionTime(const LoRaConfiguration& cfg, int payloadSize) {
        double bw_hz = cfg.bw * 1000.0;
        double t_sym = std::pow(2, cfg.sf) / bw_hz;
        double h = 0, de = 0;
        
        double theta = 8.0 * payloadSize - 4.0 * cfg.sf + 28.0 + 16.0 - 20.0 * h;
        double gamma = 4.0 * (cfg.sf - 2.0 * de);
        double n_payload = 8.0 + std::max(0.0, std::ceil(theta / gamma) * (cfg.cr + 4.0));
        
        double t_preamble = (8.0 + 4.25) * t_sym;
        double t_payload = n_payload * t_sym;
        
        return t_preamble + t_payload;
    }

private:
    Vector m_position;
    std::map<int, double> m_sfSensitivity, m_sfMinSNR;
    Ptr<NormalRandomVariable> m_shadowingRng;
    std::vector<LoRaConfiguration> m_configurations;
    std::map<uint32_t, ADRLiteDeviceState> m_adrLiteStates;
    std::map<uint32_t, NoADRDeviceState> m_noAdrStates;
    std::map<uint32_t, SNRHistory> m_snrHistories;
    std::vector<OngoingTransmission> m_currentTransmissions;
    std::map<uint32_t, bool> m_deviceMobility;
    Ptr<UniformRandomVariable> m_randomVar;
    LoRaConfiguration m_globalNoAdrConfig;
    bool m_globalNoAdrConfigInitialized = false;

    double CalculateDistance(const Vector& a, const Vector& b) const {
        return std::sqrt(std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2) + std::pow(a.z - b.z, 2));
    }
};

// --- Classe EndDevice (Noeud Final) ---

class LoRaEndDevice : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("LoRaEndDevice")
            .SetParent<Application>()
            .SetGroupName("Application");
        return tid;
    }

    LoRaEndDevice() : m_deviceId(0), m_packetInterval(30.0), m_maxMessages(100), m_sentMessages(0) {}

    void Setup(uint32_t deviceId, Ptr<LoRaGateway> gateway, ADRAlgorithm algo, int payloadSize, double mobility_speed) {
        m_deviceId = deviceId;
        m_gateway = gateway;
        m_adrAlgorithm = algo;
        m_payloadSize = payloadSize;
        m_mobilitySpeed = mobility_speed;
        m_lastConfig = m_gateway->GetDeviceConfiguration(m_deviceId, m_adrAlgorithm);
        
        if (m_adrAlgorithm == ADRAlgorithm::NO_ADR) {
            m_packetInterval = m_gateway->GetDevicePacketInterval(m_deviceId, m_adrAlgorithm);
        } else {
            m_packetInterval = 50.0;
        }
    }

    void SetMobility(Ptr<MobilityModel> mobility) {
        m_mobility = mobility;
    }

    void SendPacket() {
        // Check if we've reached the maximum number of messages
        if (m_sentMessages >= m_maxMessages) {
            return;
        }
        
        double rssi, snr;
        
        bool isMobile = false;
        if (m_mobility) {
            Vector velocity = m_mobility->GetVelocity();
            Vector currentPos = m_mobility->GetPosition();
            
            double speed = velocity.GetLength();
            isMobile = (speed > 0.05);
            
            if (!isMobile && m_packetLogs.size() > 0) {
                double timeDelta = Simulator::Now().GetSeconds() - m_packetLogs.back().time;
                if (timeDelta > 0) {
                    Vector posChange = Vector(velocity.x * timeDelta, velocity.y * timeDelta, velocity.z * timeDelta);
                    double distanceMoved = posChange.GetLength();
                    isMobile = (distanceMoved > 1.0);
                }
            }
            
            if (isMobile) {
                Ptr<UniformRandomVariable> mobilityRng = CreateObject<UniformRandomVariable>();
                mobilityRng->SetAttribute("Min", DoubleValue(0.0));
                mobilityRng->SetAttribute("Max", DoubleValue(1.0));
                isMobile = (mobilityRng->GetValue() > 0.05);
            }
        }
        
        bool success = m_gateway->ReceivePacket(m_mobility->GetPosition(), m_lastConfig, rssi, snr, m_deviceId, isMobile);
        
        PacketLog log;
        log.deviceId = m_deviceId;
        log.time = Simulator::Now().GetSeconds();
        log.success = success;
        log.rssi = rssi;
        log.snr = snr;
        log.config = m_lastConfig;
        m_packetLogs.push_back(log);

        if (m_adrAlgorithm != ADRAlgorithm::NO_ADR) {
            m_gateway->UpdateDeviceState(m_deviceId, success, snr, m_lastConfig, isMobile);
        }
        
        m_lastConfig = m_gateway->GetDeviceConfiguration(m_deviceId, m_adrAlgorithm);

        // Increment sent messages
        m_sentMessages++;
        
        // Schedule next packet only if we haven't reached max and before stop time
        if (m_sentMessages < m_maxMessages && Simulator::Now().GetSeconds() + m_packetInterval < m_stopTime.GetSeconds()) {
            Simulator::Schedule(Seconds(m_packetInterval), &LoRaEndDevice::SendPacket, this);
        }
    }

    void SetPacketInterval(double interval) { m_packetInterval = interval; }
    void SetMaxMessages(uint32_t maxMessages) { m_maxMessages = maxMessages; }

    std::vector<PacketLog> GetPacketLogs() const { return m_packetLogs; }

protected:
    virtual void StartApplication() override {
        Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
        double startDelay = rand->GetValue() * m_packetInterval;
        Simulator::Schedule(Seconds(startDelay), &LoRaEndDevice::SendPacket, this);
    }

    virtual void StopApplication() override {}

private:
    uint32_t m_deviceId;
    Ptr<LoRaGateway> m_gateway;
    Ptr<MobilityModel> m_mobility;
    ADRAlgorithm m_adrAlgorithm;
    int m_payloadSize;
    double m_mobilitySpeed;
    LoRaConfiguration m_lastConfig;
    std::vector<PacketLog> m_packetLogs;
    double m_packetInterval;
    uint32_t m_maxMessages;
    uint32_t m_sentMessages;
};

// --- Main Simulation ---

int main(int argc, char *argv[]) {
    // Paramètres de ligne de commande
    int numDevices = 100;
    double mobilitySpeed = 0.0;
    double trafficInterval = 50.0;
    double sigma = 0.0;
    std::string adrAlgoStr = "ADR-Lite";
    int runNumber = 1;
    double simulationTime = 3600.0;
    int scenario = 1;
    uint32_t maxMessages = 110;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numDevices", "Number of devices", numDevices);
    cmd.AddValue("mobilitySpeed", "Mobility speed in km/h", mobilitySpeed);
    cmd.AddValue("trafficInterval", "Traffic interval in seconds", trafficInterval);
    cmd.AddValue("sigma", "Channel saturation sigma", sigma);
    cmd.AddValue("adrAlgo", "ADR Algorithm (No-ADR, ADR-MAX, ADR-AVG, ADR-Lite)", adrAlgoStr);
    cmd.AddValue("runNumber", "Run number for repetitions", runNumber);
    cmd.AddValue("scenario", "Scenario number", scenario);
    cmd.AddValue("maxMessages", "Max messages per node (default 100)", maxMessages);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Convertir le string en ADRAlgorithm
    ADRAlgorithm adrAlgo = ADRAlgorithm::ADR_LITE;
    if (adrAlgoStr == "No-ADR") adrAlgo = ADRAlgorithm::NO_ADR;
    else if (adrAlgoStr == "ADR-MAX") adrAlgo = ADRAlgorithm::ADR_MAX;
    else if (adrAlgoStr == "ADR-AVG") adrAlgo = ADRAlgorithm::ADR_AVG;

    // Créer les répertoires nécessaires
    struct stat st = {0};
    if (stat("resultsfinal", &st) == -1) {
        mkdir("resultsfinal", 0700);
    }
    if (stat("resultsfinal/summaries", &st) == -1) {
        mkdir("resultsfinal/summaries", 0700);
    }

    // Map scenario to folder name
    std::string scenarioName = "scenario" + std::to_string(scenario);
    if (scenario == 1) scenarioName = "density";
    else if (scenario == 2) scenarioName = "mobilite";
    else if (scenario == 3) scenarioName = "sigma";
    else if (scenario == 4) scenarioName = "intervalle_d_envoie";

    std::string scenarioDir = std::string("resultsfinal/summaries/") + scenarioName;
    if (stat(scenarioDir.c_str(), &st) == -1) {
        mkdir(scenarioDir.c_str(), 0700);
    }

    // Reset Simulator
    Simulator::Destroy();

    // Créer Gateway
    Ptr<LoRaGateway> gateway = CreateObject<LoRaGateway>();
    gateway->SetChannelSaturation(sigma);
    gateway->InitializeConfigurations(20, 1);

    // Créer End Devices
    NodeContainer endDevices;
    endDevices.Create(numDevices);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=-500.0|Max=500.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=-500.0|Max=500.0]"),
                                  "Z", StringValue("ns3::UniformRandomVariable[Min=1.5|Max=1.5]"));

    if (mobilitySpeed > 0.05) {
        double speed_ms = mobilitySpeed / 3.6;
        std::stringstream speedStr;
        speedStr << "ns3::UniformRandomVariable[Min=0.0|Max=" << speed_ms << "]";
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", 
                                "Speed", StringValue(speedStr.str()), 
                                "Bounds", RectangleValue(Rectangle(-1000, 1000, -1000, 1000)));
    } else {
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }
    mobility.Install(endDevices);

    std::vector<Ptr<LoRaEndDevice>> deviceApps(numDevices);

    for (uint32_t i = 0; i < static_cast<uint32_t>(numDevices); ++i) {
        Ptr<Node> deviceNode = endDevices.Get(i);
        deviceApps[i] = CreateObject<LoRaEndDevice>();
        deviceApps[i]->Setup(i, gateway, adrAlgo, 20, mobilitySpeed);
        deviceApps[i]->SetMobility(deviceNode->GetObject<MobilityModel>());
        deviceApps[i]->SetPacketInterval(trafficInterval);
        deviceApps[i]->SetMaxMessages(maxMessages);
        deviceApps[i]->SetStartTime(Seconds(0.0));
        deviceApps[i]->SetStopTime(Seconds(simulationTime));
        deviceNode->AddApplication(deviceApps[i]);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    // Sauvegarder les résultats détaillés
    std::ostringstream filename;
    filename << "resultsfinal/sim_scen" << scenario
             << "_dev" << numDevices 
             << "_mob" << std::fixed << std::setprecision(1) << mobilitySpeed
             << "_traf" << (int)trafficInterval
             << "_sig" << std::setprecision(2) << sigma
             << "_" << adrAlgoStr
             << "_run" << runNumber << ".csv";

    std::ofstream outputFile(filename.str());
    outputFile << "DeviceId,Time,Success,RSSI,SNR,SF,TxPower,CR,CF,EnergyConsumption\n";
    
    uint32_t successfulPackets = 0;
    uint32_t totalPackets = 0;
    double totalEnergyConsumption = 0.0;

    for (uint32_t i = 0; i < static_cast<uint32_t>(numDevices); ++i) {
        for (const auto& log : deviceApps[i]->GetPacketLogs()) {
            outputFile << log.deviceId << ","
                       << log.time << ","
                       << (log.success ? "1" : "0") << ","
                       << log.rssi << ","
                       << log.snr << ","
                       << log.config.sf << ","
                       << log.config.txPower << ","
                       << log.config.cr << ","
                       << log.config.cf << ","
                       << log.config.energyConsumption << "\n";
            
            totalPackets++;
            if (log.success) {
                successfulPackets++;
                totalEnergyConsumption += log.config.energyConsumption;
            }
        }
    }
    outputFile.close();

    double pdr = (totalPackets == 0) ? 0.0 : (double)successfulPackets / totalPackets * 100.0;
    double avgEnergyPerPacket_mJ = (successfulPackets == 0) ? 0.0 : (totalEnergyConsumption / successfulPackets) * 1000.0;

    // Sauvegarder le résumé dans le dossier du scénario
    std::ostringstream summaryFilename;
    summaryFilename << scenarioDir << "/summary_scen" << scenario
                    << "_dev" << numDevices 
                    << "_mob" << std::fixed << std::setprecision(1) << mobilitySpeed
                    << "_traf" << (int)trafficInterval
                    << "_sig" << std::setprecision(2) << sigma
                    << "_" << adrAlgoStr
                    << "_run" << runNumber << ".csv";

    std::ofstream summaryFile(summaryFilename.str());
    summaryFile << "NumDevices,MobilitySpeed,TrafficInterval,Sigma,RunNumber,TotalPackets,SuccessfulPackets,PDR_Percent,AvgEnergy_mJ\n";
    summaryFile << numDevices << ","
               << std::fixed << std::setprecision(1) << mobilitySpeed << ","
               << (int)trafficInterval << ","
               << std::setprecision(2) << sigma << ","
               << runNumber << ","
               << totalPackets << ","
               << successfulPackets << ","
               << std::setprecision(2) << pdr << ","
               << std::setprecision(6) << avgEnergyPerPacket_mJ << "\n";
    summaryFile.close();

    std::cout << "Run " << runNumber << " (" << adrAlgoStr << "): PDR=" 
              << std::fixed << std::setprecision(2) << pdr 
              << "%, Energy=" << std::setprecision(6) << avgEnergyPerPacket_mJ 
              << " mJ, Messages=" << totalPackets << std::endl;

    return 0;
}