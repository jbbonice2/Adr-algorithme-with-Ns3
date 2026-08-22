/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * UCB1-Tuned agent implementation.
 */

#include "ucb1-agent.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("Ucb1Agent");
NS_OBJECT_ENSURE_REGISTERED(Ucb1Agent);

// --- ArmStats helpers ---

double
Ucb1Agent::ArmStats::GetMean() const
{
    return (selectionsCount > 0) ? (rewardsSum / selectionsCount) : 0.0;
}

double
Ucb1Agent::ArmStats::GetVariance() const
{
    if (selectionsCount < 2)
    {
        return 0.0;
    }
    double mean = GetMean();
    double sumSqDev = 0.0;
    for (double r : rewardHistory)
    {
        double diff = r - mean;
        sumSqDev += diff * diff;
    }
    return sumSqDev / selectionsCount;
}

// --- Ucb1Agent ---

TypeId
Ucb1Agent::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Ucb1Agent")
            .SetGroupName("lorawan")
            .AddConstructor<Ucb1Agent>()
            .SetParent<Object>()
            .AddAttribute("PayloadSize",
                          "Default payload size in bytes for reward normalisation",
                          UintegerValue(20),
                          MakeUintegerAccessor(&Ucb1Agent::m_payloadSize),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

Ucb1Agent::Ucb1Agent()
    : m_totalSelections(0),
      m_payloadSize(20)
{
    m_rng = CreateObject<UniformRandomVariable>();

    m_sfSet = {7, 8, 9, 10, 11, 12};
    m_tpSet = {2, 5, 8, 11, 14};

    InitializeArms();
}

void
Ucb1Agent::InitializeArms()
{
    m_armStats.clear();
    for (int sf : m_sfSet)
    {
        for (double tp : m_tpSet)
        {
            m_armStats[{sf, tp}] = ArmStats();
        }
    }
}

void
Ucb1Agent::SetPayloadSize(uint32_t bytes)
{
    m_payloadSize = bytes;
}

void
Ucb1Agent::SetSfSet(const std::vector<int>& sfSet)
{
    m_sfSet = sfSet;
    InitializeArms();
}

void
Ucb1Agent::SetTpSet(const std::vector<double>& tpSet)
{
    m_tpSet = tpSet;
    InitializeArms();
}

std::pair<int, double>
Ucb1Agent::SelectParameters()
{
    m_totalSelections++;

    // Exploration phase: try each arm at least once
    for (int sf : m_sfSet)
    {
        for (double tp : m_tpSet)
        {
            if (m_armStats[{sf, tp}].selectionsCount == 0)
            {
                NS_LOG_DEBUG("UCB1 explore: SF=" << sf << " TP=" << tp);
                return {sf, tp};
            }
        }
    }

    // Exploitation phase: select arm with highest UCB1-Tuned score
    double bestScore = -std::numeric_limits<double>::infinity();
    int bestSF = m_sfSet[0];
    double bestTP = m_tpSet[0];

    for (int sf : m_sfSet)
    {
        for (double tp : m_tpSet)
        {
            double score = CalculateUcbScore(sf, tp);
            if (score > bestScore)
            {
                bestScore = score;
                bestSF = sf;
                bestTP = tp;
            }
        }
    }

    NS_LOG_DEBUG("UCB1 exploit: SF=" << bestSF << " TP=" << bestTP
                                     << " score=" << bestScore);
    return {bestSF, bestTP};
}

void
Ucb1Agent::UpdateRewards(int sf, double tp, bool success)
{
    auto key = std::make_pair(sf, tp);
    ArmStats& stats = m_armStats[key];

    // Reward: 1/ToA on success, 0 on failure
    double reward = 0.0;
    if (success)
    {
        double BW = 125000.0;
        double symbolTime = std::pow(2, sf) / BW;
        double ToA = symbolTime * (8 + m_payloadSize);
        reward = 1.0 / ToA;
    }

    stats.rewardsSum += reward;
    stats.selectionsCount++;
    stats.rewardHistory.push_back(reward);
}

double
Ucb1Agent::CalculateUcbScore(int sf, double tp)
{
    auto key = std::make_pair(sf, tp);
    ArmStats& stats = m_armStats[key];

    if (stats.selectionsCount == 0)
    {
        return std::numeric_limits<double>::infinity();
    }

    double meanReward = stats.GetMean();
    double variance = stats.GetVariance();

    // V_i = σ² + sqrt(2·ln(t)/N_i)
    double V_ui = variance +
                  std::sqrt(2.0 * std::log(m_totalSelections) / stats.selectionsCount);

    // UCB1-Tuned score: μ + sqrt(ln(t)/N_i · min(1/4, V_i))
    double explorationTerm =
        std::sqrt((std::log(m_totalSelections) / stats.selectionsCount) *
                  std::min(0.25, V_ui));

    return meanReward + explorationTerm;
}

} // namespace lorawan
} // namespace ns3
