/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Tug-of-War (ToW) agent for joint channel and SF selection in LoRaWAN.
 */

#ifndef TOW_AGENT_H
#define TOW_AGENT_H

#include "ns3/object.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace ns3
{
namespace lorawan
{

/**
 * \brief Tug-of-War (ToW) dynamics agent for channel and SF selection.
 *
 * Maintains dual Q-value tables for channels and spreading factors.
 * Selection uses an oscillation term A·cos(2π(t+arm)/K) to promote
 * exploration.  Reward updates apply α-decay on success and a penalty
 * derived from empirical success-rate gaps on failure.  Counters are
 * decayed with a β-forgetting factor.
 */
class ToWAgent : public Object
{
  public:
    static TypeId GetTypeId();

    ToWAgent();

    /**
     * \brief Set the number of channels and SF options.
     * \param numChannels Number of available channels.
     * \param numSF       Number of available spreading factors.
     */
    void SetDimensions(uint32_t numChannels, uint32_t numSF);

    /**
     * \brief Set ToW hyperparameters.
     * \param alpha Q-value decay factor (default 0.9).
     * \param beta  Counter forgetting factor (default 0.9).
     * \param A     Oscillation amplitude (default 0.5).
     */
    void SetParameters(double alpha, double beta, double A);

    /**
     * \brief Select (channel, sfIndex) using ToW dynamics.
     * \return Pair of (channel index, SF index in [0, numSF)).
     *
     * During the first max(numChannels, numSF) rounds the selection
     * is uniformly random for initialisation.
     */
    std::pair<uint32_t, uint32_t> SelectParameters();

    /**
     * \brief Update Q-values after transmission outcome.
     * \param channel  Channel index used.
     * \param sfIdx    SF index used (0 = SF7, …, 5 = SF12).
     * \param success  Whether the packet was successfully received.
     */
    void UpdateReward(uint32_t channel, uint32_t sfIdx, bool success);

  private:
    double CalculateX(uint32_t arm, bool isChannel) const;
    double CalculatePenalty() const;

    uint32_t m_numChannels;
    uint32_t m_numSF;
    double m_alpha;
    double m_beta;
    double m_A;
    uint32_t m_totalTime;

    std::vector<double> m_Q_ch;
    std::vector<double> m_Q_sf;
    std::vector<double> m_N_ch;
    std::vector<double> m_N_sf;
    std::vector<uint32_t> m_R_ch;
    std::vector<uint32_t> m_R_sf;
};

} // namespace lorawan
} // namespace ns3

#endif // TOW_AGENT_H
