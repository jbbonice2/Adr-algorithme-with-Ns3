/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * D-LoRa: Decentralized LoRa parameter selection using UCB multi-armed bandit.
 *
 * Each arm dimension (SF, BW, TP) is treated as an independent MAB.
 * Reward functions incorporate throughput (ξ), bandwidth (ζ), and
 * energy-efficiency (η) objectives.
 */

#ifndef DLORA_AGENT_H
#define DLORA_AGENT_H

#include "ns3/object.h"
#include "ns3/random-variable-stream.h"

#include <cstdint>
#include <map>
#include <vector>

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * D-LoRa agent: UCB-based multi-armed bandit for decentralised
 * LoRa transmission parameter selection (SF, BW, TP).
 *
 * Three independent MAB instances select SF, BW, and TP.
 * The exploration constant C controls the exploitation/exploration trade-off.
 */
class DLoRaAgent : public Object
{
  public:
    static TypeId GetTypeId();

    DLoRaAgent();
    ~DLoRaAgent() override = default;

    /**
     * Select (SF, BW, TP) using UCB exploration/exploitation.
     * @return tuple of (SF, BW_Hz, TP_dBm)
     */
    std::tuple<int, double, double> SelectParameters();

    /**
     * Update rewards after observing transmission outcome.
     * @param sf  Spreading Factor used
     * @param bw  Bandwidth used (Hz)
     * @param tp  Transmission power used (dBm)
     * @param success true if packet was received at the gateway
     */
    void UpdateRewards(int sf, double bw, double tp, bool success);

    // Variant weight setters
    void SetXi(double xi);     //!< Throughput weight
    void SetZeta(double zeta); //!< Bandwidth weight
    void SetEta(double eta);   //!< Energy-efficiency weight
    void SetVariantWeights(double xi, double zeta, double eta);

    // Parameter space configuration
    void SetSfSet(const std::vector<int>& sfSet);
    void SetBwSet(const std::vector<double>& bwSet);
    void SetTpSet(const std::vector<double>& tpSet);
    void SetExplorationConstant(double c);

  private:
    void InitializeArms();

    template <typename T>
    T SelectArm(std::map<T, double>& expectedRewards,
                std::map<T, uint32_t>& numSelections,
                const std::vector<T>& armSet);

    template <typename T>
    void UpdateArm(std::map<T, double>& expectedRewards,
                   std::map<T, uint32_t>& numSelections,
                   T arm,
                   double reward);

    double CalculateRewardSF(int sf, bool success);
    double CalculateRewardBW(double bw, bool success);
    double CalculateRewardTP(double tp, bool success);

    Ptr<UniformRandomVariable> m_rng;
    uint32_t m_totalSelections;

    // Per-arm state
    std::map<int, double> m_expectedRewardsSF;
    std::map<int, uint32_t> m_numSelectionsSF;
    std::map<double, double> m_expectedRewardsBW;
    std::map<double, uint32_t> m_numSelectionsBW;
    std::map<double, double> m_expectedRewardsTP;
    std::map<double, uint32_t> m_numSelectionsTP;

    // Parameter space
    std::vector<int> m_sfSet;
    std::vector<double> m_bwSet;
    std::vector<double> m_tpSet;

    // Variant weights
    double m_xi;   //!< Throughput weight (ξ)
    double m_zeta; //!< Bandwidth weight (ζ)
    double m_eta;  //!< Energy-efficiency weight (η)

    double m_explorationConstant; //!< C in UCB formula
};

} // namespace lorawan
} // namespace ns3

#endif // DLORA_AGENT_H
