/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * QoC-A / DQoC-A agent implementation.
 */

#include "qoca-agent.h"

#include "ns3/log.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("QoCaAgent");
NS_OBJECT_ENSURE_REGISTERED(QoCaAgent);

TypeId
QoCaAgent::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::QoCaAgent")
            .SetGroupName("lorawan")
            .AddConstructor<QoCaAgent>()
            .SetParent<Object>();
    return tid;
}

QoCaAgent::QoCaAgent()
    : m_K(8),
      m_n(0),
      m_currentChannel(0),
      m_alpha(1.9),
      m_beta(0.9),
      m_lambda(0.98),
      m_lambdaG(0.90),
      m_type(QOC_A)
{
    m_T_i.resize(m_K, 0);
    m_R_i.resize(m_K, 0.0);
    m_G_i.resize(m_K, 0.0);
    m_rewards.resize(m_K);
    m_qualities.resize(m_K);
}

void
QoCaAgent::SetAlgorithmType(AlgorithmType type)
{
    m_type = type;
}

QoCaAgent::AlgorithmType
QoCaAgent::GetAlgorithmType() const
{
    return m_type;
}

std::string
QoCaAgent::GetTypeName() const
{
    switch (m_type)
    {
    case UNIFORM:
        return "Uniform";
    case UCB:
        return "UCB";
    case QOC_A:
        return "QoC-A";
    case DQOC_A:
        return "DQoC-A";
    default:
        return "Unknown";
    }
}

void
QoCaAgent::SetParameters(double alpha, double beta, double lambda, double lambdaG)
{
    m_alpha = alpha;
    m_beta = beta;
    m_lambda = lambda;
    m_lambdaG = lambdaG;
}

void
QoCaAgent::SetNumChannels(uint32_t K)
{
    m_K = K;
    m_T_i.assign(m_K, 0);
    m_R_i.assign(m_K, 0.0);
    m_G_i.assign(m_K, 0.0);
    m_rewards.assign(m_K, {});
    m_qualities.assign(m_K, {});
    m_channelHistory.clear();
    m_n = 0;
    m_currentChannel = 0;
}

uint32_t
QoCaAgent::SelectChannel()
{
    m_n++;

    switch (m_type)
    {
    case UNIFORM:
        return SelectChannelUniform();
    case UCB:
        return SelectChannelUCB();
    case QOC_A:
        return SelectChannelQoCA();
    case DQOC_A:
        return SelectChannelDQoCA();
    default:
        return 0;
    }
}

void
QoCaAgent::UpdateReward(uint32_t channel, bool success, double quality)
{
    double reward = success ? 1.0 : 0.0;

    m_T_i[channel]++;
    m_rewards[channel].push_back(reward);
    m_qualities[channel].push_back(quality);
    m_channelHistory.push_back(channel);

    UpdateEmpiricalMeans(channel);
}

// --- Private selection strategies ---

uint32_t
QoCaAgent::SelectChannelUniform()
{
    uint32_t ch = m_currentChannel;
    m_currentChannel = (m_currentChannel + 1) % m_K;
    return ch;
}

uint32_t
QoCaAgent::SelectChannelUCB()
{
    if (m_n <= m_K)
    {
        return (m_n - 1);
    }

    double maxUCB = -std::numeric_limits<double>::infinity();
    uint32_t bestChannel = 0;

    for (uint32_t i = 0; i < m_K; i++)
    {
        if (m_T_i[i] == 0)
        {
            return i;
        }

        double B_i = m_R_i[i] + m_alpha * std::sqrt(std::log(m_n) / m_T_i[i]);

        if (B_i > maxUCB)
        {
            maxUCB = B_i;
            bestChannel = i;
        }
    }

    return bestChannel;
}

uint32_t
QoCaAgent::SelectChannelQoCA()
{
    if (m_n <= m_K)
    {
        return (m_n - 1);
    }

    double maxScore = -std::numeric_limits<double>::infinity();
    uint32_t bestChannel = 0;
    double G_max = CalculateGmax();

    for (uint32_t i = 0; i < m_K; i++)
    {
        if (m_T_i[i] == 0)
        {
            return i;
        }

        double Q_i = 0.0;
        if (G_max > 0.0)
        {
            Q_i = m_beta * (m_G_i[i] / G_max - 1.0) *
                  std::log(m_n) / m_T_i[i];
        }

        double B_i = m_R_i[i] + Q_i +
                     m_alpha * std::sqrt(std::log(m_n) / m_T_i[i]);

        if (B_i > maxScore)
        {
            maxScore = B_i;
            bestChannel = i;
        }
    }

    return bestChannel;
}

uint32_t
QoCaAgent::SelectChannelDQoCA()
{
    if (m_n <= m_K)
    {
        return (m_n - 1);
    }

    // Discounted statistics
    std::vector<double> N_i(m_K, 0.0);
    double W_n = 0.0;
    std::vector<double> R_disc(m_K, 0.0);
    std::vector<double> G_disc(m_K, 0.0);

    for (uint32_t i = 0; i < m_K; i++)
    {
        double sum_disc = 0.0;
        double sum_r = 0.0;
        double sum_g = 0.0;

        for (size_t j = 0; j < m_channelHistory.size(); j++)
        {
            if (m_channelHistory[j] == i)
            {
                double discount =
                    std::pow(m_lambda, m_channelHistory.size() - 1 - j);
                sum_disc += discount;

                if (j < m_rewards[i].size())
                {
                    sum_r += discount * m_rewards[i][j];
                }
                if (j < m_qualities[i].size())
                {
                    sum_g += discount * m_qualities[i][j];
                }
            }
        }
        N_i[i] = sum_disc;
        W_n += sum_disc;

        if (N_i[i] > 0)
        {
            R_disc[i] = sum_r / N_i[i];
            G_disc[i] = sum_g / N_i[i];
        }
    }

    double G_max = *std::max_element(G_disc.begin(), G_disc.end());

    double maxScore = -std::numeric_limits<double>::infinity();
    uint32_t bestChannel = 0;

    for (uint32_t i = 0; i < m_K; i++)
    {
        if (N_i[i] == 0)
        {
            return i;
        }

        double Q_i = 0.0;
        if (G_max > 0.0)
        {
            Q_i = m_beta * (G_disc[i] / G_max - 1.0) *
                  std::log(W_n) / N_i[i];
        }

        double B_i = R_disc[i] + Q_i +
                     m_alpha * std::sqrt(std::log(W_n) / N_i[i]);

        if (B_i > maxScore)
        {
            maxScore = B_i;
            bestChannel = i;
        }
    }

    return bestChannel;
}

void
QoCaAgent::UpdateEmpiricalMeans(uint32_t channel)
{
    double sum_r = 0.0;
    for (double r : m_rewards[channel])
    {
        sum_r += r;
    }
    m_R_i[channel] = (m_T_i[channel] > 0) ? sum_r / m_T_i[channel] : 0.0;

    double sum_g = 0.0;
    for (double g : m_qualities[channel])
    {
        sum_g += g;
    }
    m_G_i[channel] = (m_T_i[channel] > 0) ? sum_g / m_T_i[channel] : 0.0;
}

double
QoCaAgent::CalculateGmax() const
{
    double maxG = 0.0;
    for (uint32_t i = 0; i < m_K; i++)
    {
        if (m_G_i[i] > maxG)
        {
            maxG = m_G_i[i];
        }
    }
    return maxG;
}

} // namespace lorawan
} // namespace ns3
