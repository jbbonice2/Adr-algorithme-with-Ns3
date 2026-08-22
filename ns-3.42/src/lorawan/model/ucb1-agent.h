/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * UCB1-Tuned agent for LoRaWAN joint SF/TP selection.
 *
 * Selects from the Cartesian product of (SF, TP) pairs using
 * the UCB1-Tuned bandit algorithm, which incorporates variance
 * estimation to improve the confidence bound.
 */

#ifndef UCB1_AGENT_H
#define UCB1_AGENT_H

#include "ns3/object.h"
#include "ns3/random-variable-stream.h"

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * UCB1-Tuned agent for joint (SF, TP) arm selection.
 *
 * Each arm is a (SF, TP) pair. The score for arm i at round t is:
 *   UCB_i = μ_i + sqrt( ln(t) / N_i × min(1/4, V_i) )
 * where V_i = σ²_i + sqrt(2 ln(t) / N_i) is the tuned variance term.
 */
class Ucb1Agent : public Object
{
  public:
    static TypeId GetTypeId();

    Ucb1Agent();
    ~Ucb1Agent() override = default;

    /**
     * Select (SF, TP) using UCB1-Tuned.
     * @return pair of (SF, TP_dBm)
     */
    std::pair<int, double> SelectParameters();

    /**
     * Update the arm reward after observing outcome.
     * @param sf   Spreading Factor used
     * @param tp   Transmission power used (dBm)
     * @param success true if packet received at gateway
     */
    void UpdateRewards(int sf, double tp, bool success);

    /** Configure default payload size used for reward normalisation. */
    void SetPayloadSize(uint32_t bytes);

    /** Configure the parameter space. */
    void SetSfSet(const std::vector<int>& sfSet);
    void SetTpSet(const std::vector<double>& tpSet);

  private:
    /** Per-arm statistics for UCB1-Tuned. */
    struct ArmStats
    {
        double rewardsSum;
        uint32_t selectionsCount;
        std::vector<double> rewardHistory;

        ArmStats()
            : rewardsSum(0.0),
              selectionsCount(0)
        {
        }

        double GetMean() const;
        double GetVariance() const;
    };

    void InitializeArms();
    double CalculateUcbScore(int sf, double tp);

    Ptr<UniformRandomVariable> m_rng;
    uint32_t m_totalSelections;
    uint32_t m_payloadSize;

    std::map<std::pair<int, double>, ArmStats> m_armStats;

    std::vector<int> m_sfSet;
    std::vector<double> m_tpSet;
};

} // namespace lorawan
} // namespace ns3

#endif // UCB1_AGENT_H
