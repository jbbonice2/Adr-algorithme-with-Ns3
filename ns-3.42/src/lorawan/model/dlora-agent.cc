/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * D-LoRa agent implementation.
 */

#include "dlora-agent.h"

#include "ns3/double.h"
#include "ns3/log.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("DLoRaAgent");
NS_OBJECT_ENSURE_REGISTERED(DLoRaAgent);

TypeId
DLoRaAgent::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DLoRaAgent")
            .SetGroupName("lorawan")
            .AddConstructor<DLoRaAgent>()
            .SetParent<Object>()
            .AddAttribute("ExplorationConstant",
                          "UCB exploration constant C",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&DLoRaAgent::m_explorationConstant),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("Xi",
                          "Throughput variant weight (ξ)",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&DLoRaAgent::m_xi),
                          MakeDoubleChecker<double>())
            .AddAttribute("Zeta",
                          "Bandwidth variant weight (ζ)",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&DLoRaAgent::m_zeta),
                          MakeDoubleChecker<double>())
            .AddAttribute("Eta",
                          "Energy-efficiency variant weight (η)",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&DLoRaAgent::m_eta),
                          MakeDoubleChecker<double>());
    return tid;
}

DLoRaAgent::DLoRaAgent()
    : m_totalSelections(0),
      m_xi(0.0),
      m_zeta(0.0),
      m_eta(0.0),
      m_explorationConstant(2.0)
{
    m_rng = CreateObject<UniformRandomVariable>();

    // Default parameter space (EU868)
    m_sfSet = {7, 8, 9, 10, 11, 12};
    m_bwSet = {125000};
    m_tpSet = {2, 5, 8, 11, 14};

    InitializeArms();
}

void
DLoRaAgent::InitializeArms()
{
    m_expectedRewardsSF.clear();
    m_numSelectionsSF.clear();
    for (int sf : m_sfSet)
    {
        m_expectedRewardsSF[sf] = 0.0;
        m_numSelectionsSF[sf] = 0;
    }

    m_expectedRewardsBW.clear();
    m_numSelectionsBW.clear();
    for (double bw : m_bwSet)
    {
        m_expectedRewardsBW[bw] = 0.0;
        m_numSelectionsBW[bw] = 0;
    }

    m_expectedRewardsTP.clear();
    m_numSelectionsTP.clear();
    for (double tp : m_tpSet)
    {
        m_expectedRewardsTP[tp] = 0.0;
        m_numSelectionsTP[tp] = 0;
    }
}

void
DLoRaAgent::SetSfSet(const std::vector<int>& sfSet)
{
    m_sfSet = sfSet;
    InitializeArms();
}

void
DLoRaAgent::SetBwSet(const std::vector<double>& bwSet)
{
    m_bwSet = bwSet;
    InitializeArms();
}

void
DLoRaAgent::SetTpSet(const std::vector<double>& tpSet)
{
    m_tpSet = tpSet;
    InitializeArms();
}

void
DLoRaAgent::SetExplorationConstant(double c)
{
    m_explorationConstant = c;
}

void
DLoRaAgent::SetXi(double xi)
{
    m_xi = xi;
}

void
DLoRaAgent::SetZeta(double zeta)
{
    m_zeta = zeta;
}

void
DLoRaAgent::SetEta(double eta)
{
    m_eta = eta;
}

void
DLoRaAgent::SetVariantWeights(double xi, double zeta, double eta)
{
    m_xi = xi;
    m_zeta = zeta;
    m_eta = eta;
}

std::tuple<int, double, double>
DLoRaAgent::SelectParameters()
{
    m_totalSelections++;

    int selectedSF = SelectArm(m_expectedRewardsSF, m_numSelectionsSF, m_sfSet);
    double selectedBW = SelectArm(m_expectedRewardsBW, m_numSelectionsBW, m_bwSet);
    double selectedTP = SelectArm(m_expectedRewardsTP, m_numSelectionsTP, m_tpSet);

    NS_LOG_DEBUG("D-LoRa selected: SF=" << selectedSF << " BW=" << selectedBW
                                        << " TP=" << selectedTP);

    return std::make_tuple(selectedSF, selectedBW, selectedTP);
}

void
DLoRaAgent::UpdateRewards(int sf, double bw, double tp, bool success)
{
    double rewardSF = CalculateRewardSF(sf, success);
    double rewardBW = CalculateRewardBW(bw, success);
    double rewardTP = CalculateRewardTP(tp, success);

    UpdateArm(m_expectedRewardsSF, m_numSelectionsSF, sf, rewardSF);
    UpdateArm(m_expectedRewardsBW, m_numSelectionsBW, bw, rewardBW);
    UpdateArm(m_expectedRewardsTP, m_numSelectionsTP, tp, rewardTP);
}

template <typename T>
T
DLoRaAgent::SelectArm(std::map<T, double>& expectedRewards,
                       std::map<T, uint32_t>& numSelections,
                       const std::vector<T>& armSet)
{
    double maxUCB = -1.0;
    T selectedArm = armSet[0];

    uint32_t totalSelections = 0;
    for (auto const& [key, val] : numSelections)
    {
        totalSelections += val;
    }

    for (T arm : armSet)
    {
        double ucbValue;
        if (numSelections[arm] == 0)
        {
            ucbValue = std::numeric_limits<double>::max();
        }
        else
        {
            ucbValue = expectedRewards[arm] +
                       m_explorationConstant *
                           std::sqrt(std::log(totalSelections + 1) /
                                     (2.0 * numSelections[arm]));
        }

        if (ucbValue > maxUCB)
        {
            maxUCB = ucbValue;
            selectedArm = arm;
        }
    }
    return selectedArm;
}

template <typename T>
void
DLoRaAgent::UpdateArm(std::map<T, double>& expectedRewards,
                       std::map<T, uint32_t>& numSelections,
                       T arm,
                       double reward)
{
    numSelections[arm]++;
    expectedRewards[arm] +=
        (reward - expectedRewards[arm]) / numSelections[arm];
}

double
DLoRaAgent::CalculateRewardSF(int sf, bool success)
{
    double r_sf = success ? 1.0 : 0.0;
    if (m_xi > 0)
    {
        double sum_2_sf = 0;
        for (int s : m_sfSet)
        {
            sum_2_sf += std::pow(2, s);
        }
        r_sf += m_xi * (std::pow(2, sf) / sum_2_sf);
    }
    return r_sf;
}

double
DLoRaAgent::CalculateRewardBW(double bw, bool success)
{
    double r_bw = success ? 1.0 : 0.0;
    if (m_zeta > 0)
    {
        double sum_bw = 0;
        for (double b : m_bwSet)
        {
            sum_bw += b;
        }
        r_bw += m_zeta * (bw / sum_bw);
    }
    return r_bw;
}

double
DLoRaAgent::CalculateRewardTP(double tp, bool success)
{
    double r_tp = success ? 1.0 : 0.0;
    if (m_eta > 0)
    {
        double sum_tp = 0;
        for (double t : m_tpSet)
        {
            sum_tp += t;
        }
        r_tp += m_eta * (1.0 - (tp / sum_tp));
    }
    return r_tp;
}

} // namespace lorawan
} // namespace ns3
