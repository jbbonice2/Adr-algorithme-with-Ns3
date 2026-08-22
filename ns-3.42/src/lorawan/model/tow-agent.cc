/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Tug-of-War (ToW) agent implementation.
 */

#include "tow-agent.h"

#include "ns3/log.h"
#include "ns3/random-variable-stream.h"

#include <algorithm>
#include <cmath>

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("ToWAgent");
NS_OBJECT_ENSURE_REGISTERED(ToWAgent);

TypeId
ToWAgent::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ToWAgent")
            .SetGroupName("lorawan")
            .AddConstructor<ToWAgent>()
            .SetParent<Object>();
    return tid;
}

ToWAgent::ToWAgent()
    : m_numChannels(8),
      m_numSF(6),
      m_alpha(0.9),
      m_beta(0.9),
      m_A(0.5),
      m_totalTime(0)
{
    m_Q_ch.resize(m_numChannels, 0.0);
    m_Q_sf.resize(m_numSF, 0.0);
    m_N_ch.resize(m_numChannels, 0.0);
    m_N_sf.resize(m_numSF, 0.0);
    m_R_ch.resize(m_numChannels, 0);
    m_R_sf.resize(m_numSF, 0);
}

void
ToWAgent::SetDimensions(uint32_t numChannels, uint32_t numSF)
{
    m_numChannels = numChannels;
    m_numSF = numSF;
    m_Q_ch.assign(m_numChannels, 0.0);
    m_Q_sf.assign(m_numSF, 0.0);
    m_N_ch.assign(m_numChannels, 0.0);
    m_N_sf.assign(m_numSF, 0.0);
    m_R_ch.assign(m_numChannels, 0);
    m_R_sf.assign(m_numSF, 0);
    m_totalTime = 0;
}

void
ToWAgent::SetParameters(double alpha, double beta, double A)
{
    m_alpha = alpha;
    m_beta = beta;
    m_A = A;
}

std::pair<uint32_t, uint32_t>
ToWAgent::SelectParameters()
{
    m_totalTime++;

    // Random selection for initial rounds
    if (m_totalTime <= std::max(m_numChannels, m_numSF))
    {
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        uint32_t ch = rng->GetInteger(0, m_numChannels - 1);
        uint32_t sfIdx = rng->GetInteger(0, m_numSF - 1);
        return std::make_pair(ch, sfIdx);
    }

    // ToW channel selection
    uint32_t bestChannel = 0;
    double maxX_ch = CalculateX(0, true);
    for (uint32_t ch = 1; ch < m_numChannels; ch++)
    {
        double x = CalculateX(ch, true);
        if (x > maxX_ch)
        {
            maxX_ch = x;
            bestChannel = ch;
        }
    }

    // ToW SF selection
    uint32_t bestSFIdx = 0;
    double maxX_sf = CalculateX(0, false);
    for (uint32_t sf = 1; sf < m_numSF; sf++)
    {
        double x = CalculateX(sf, false);
        if (x > maxX_sf)
        {
            maxX_sf = x;
            bestSFIdx = sf;
        }
    }

    return std::make_pair(bestChannel, bestSFIdx);
}

void
ToWAgent::UpdateReward(uint32_t channel, uint32_t sfIdx, bool success)
{
    if (success)
    {
        m_Q_ch[channel] = m_alpha * m_Q_ch[channel] + 1.0;
        m_Q_sf[sfIdx] = m_alpha * m_Q_sf[sfIdx] + 1.0;
        m_R_ch[channel]++;
        m_R_sf[sfIdx]++;
    }
    else
    {
        double penalty = CalculatePenalty();
        m_Q_ch[channel] = m_alpha * m_Q_ch[channel] - penalty;
        m_Q_sf[sfIdx] = m_alpha * m_Q_sf[sfIdx] - penalty;
    }

    // Update counters with forgetting factor
    for (uint32_t i = 0; i < m_numChannels; i++)
    {
        if (i == channel)
        {
            m_N_ch[i] = 1.0 + m_beta * m_N_ch[i];
        }
        else
        {
            m_N_ch[i] = m_beta * m_N_ch[i];
        }
    }

    for (uint32_t i = 0; i < m_numSF; i++)
    {
        if (i == sfIdx)
        {
            m_N_sf[i] = 1.0 + m_beta * m_N_sf[i];
        }
        else
        {
            m_N_sf[i] = m_beta * m_N_sf[i];
        }
    }
}

// --- Private helpers ---

double
ToWAgent::CalculateX(uint32_t arm, bool isChannel) const
{
    if (isChannel)
    {
        double Q_k = m_Q_ch[arm];
        double sum_others = 0.0;
        for (uint32_t i = 0; i < m_numChannels; i++)
        {
            if (i != arm)
            {
                sum_others += m_Q_ch[i];
            }
        }
        double avg_others =
            (m_numChannels > 1) ? sum_others / (m_numChannels - 1) : 0.0;
        double osc =
            m_A * std::cos(2.0 * M_PI * (m_totalTime + arm) / m_numChannels);
        return Q_k - avg_others + osc;
    }
    else
    {
        double Q_k = m_Q_sf[arm];
        double sum_others = 0.0;
        for (uint32_t i = 0; i < m_numSF; i++)
        {
            if (i != arm)
            {
                sum_others += m_Q_sf[i];
            }
        }
        double avg_others =
            (m_numSF > 1) ? sum_others / (m_numSF - 1) : 0.0;
        double osc =
            m_A * std::cos(2.0 * M_PI * (m_totalTime + arm) / m_numSF);
        return Q_k - avg_others + osc;
    }
}

double
ToWAgent::CalculatePenalty() const
{
    std::vector<double> probs;
    for (uint32_t i = 0; i < m_numChannels; i++)
    {
        if (m_N_ch[i] > 0)
        {
            probs.push_back(static_cast<double>(m_R_ch[i]) / m_N_ch[i]);
        }
    }

    if (probs.size() < 2)
    {
        return 0.1;
    }

    std::sort(probs.rbegin(), probs.rend());
    double p1 = probs[0];
    double p2 = probs[1];

    if (p1 == p2)
    {
        return 0.1;
    }
    return (p1 + p2) / 2.0 - (p1 - p2);
}

} // namespace lorawan
} // namespace ns3
