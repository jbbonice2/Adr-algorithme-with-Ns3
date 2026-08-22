/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * QoC-A / DQoC-A: Quality of Channel Allocation agents for LoRaWAN.
 *
 * QoC-A augments the UCB index with a quality-of-channel penalty term.
 * DQoC-A adds exponential discounting for non-stationary environments.
 */

#ifndef QOCA_AGENT_H
#define QOCA_AGENT_H

#include "ns3/object.h"
#include "ns3/random-variable-stream.h"

#include <cstdint>
#include <vector>

namespace ns3
{
namespace lorawan
{

/**
 * @ingroup lorawan
 *
 * QoC-A / DQoC-A channel selection agent.
 *
 * Supports four modes via AlgorithmType:
 *   - UNIFORM:  round-robin channel selection (baseline)
 *   - UCB:      standard UCB1 channel selection
 *   - QOC_A:    UCB + quality-of-channel penalty
 *   - DQOC_A:   discounted QoC-A for non-stationary channels
 */
class QoCaAgent : public Object
{
  public:
    enum AlgorithmType
    {
        UNIFORM,
        UCB,
        QOC_A,
        DQOC_A,
    };

    static TypeId GetTypeId();

    QoCaAgent();
    ~QoCaAgent() override = default;

    void SetAlgorithmType(AlgorithmType type);
    AlgorithmType GetAlgorithmType() const;
    std::string GetTypeName() const;

    /**
     * Set algorithm hyper-parameters.
     * @param alpha   exploration weight (default 1.9)
     * @param beta    quality penalty weight (default 0.9)
     * @param lambda  discount factor for DQoC-A rewards (default 0.98)
     * @param lambdaG discount factor for DQoC-A qualities (default 0.90)
     */
    void SetParameters(double alpha,
                       double beta,
                       double lambda = 0.98,
                       double lambdaG = 0.90);

    void SetNumChannels(uint32_t K);

    /**
     * Select the next channel to use.
     * @return channel index [0, K)
     */
    uint32_t SelectChannel();

    /**
     * Observe reward for the selected channel.
     * @param channel  the channel used
     * @param success  true if packet delivered
     * @param quality  channel quality metric (e.g. SNR-based)
     */
    void UpdateReward(uint32_t channel, bool success, double quality);

  private:
    uint32_t SelectChannelUniform();
    uint32_t SelectChannelUCB();
    uint32_t SelectChannelQoCA();
    uint32_t SelectChannelDQoCA();

    void UpdateEmpiricalMeans(uint32_t channel);
    double CalculateGmax() const;

    uint32_t m_K;              //!< Number of channels
    uint32_t m_n;              //!< Total rounds played
    uint32_t m_currentChannel; //!< Round-robin pointer
    double m_alpha;            //!< Exploration weight
    double m_beta;             //!< Quality penalty weight
    double m_lambda;           //!< Discount factor (DQoC-A rewards)
    double m_lambdaG;          //!< Discount factor (DQoC-A qualities)
    AlgorithmType m_type;

    std::vector<uint32_t> m_T_i;               //!< Pull counts per channel
    std::vector<double> m_R_i;                  //!< Empirical mean reward
    std::vector<double> m_G_i;                  //!< Empirical mean quality
    std::vector<std::vector<double>> m_rewards; //!< Reward history per channel
    std::vector<std::vector<double>> m_qualities; //!< Quality history per channel
    std::vector<uint32_t> m_channelHistory;     //!< Sequence of channel selections
};

} // namespace lorawan
} // namespace ns3

#endif // QOCA_AGENT_H
