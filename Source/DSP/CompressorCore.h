#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace kmp
{

struct CompressorParams
{
    float thresholdDb   = -20.0f;
    float ratio         = 4.0f;
    float attackMs      = 10.0f;
    float releaseMs     = 100.0f;
    float kneeDb        = 6.0f;
    float rangeDb       = 60.0f;
    float makeupDb      = 0.0f;
    float mix           = 1.0f;
    float lookaheadMs   = 0.0f;
    float hpfHz         = 120.0f;
    bool  hpfOn         = false;
    bool  rmsMode       = false;
    bool  autoRelease   = false;
};

inline float gainToDb (float g) noexcept
{
    return g > 1.0e-6f ? 20.0f * std::log10 (g) : -120.0f;
}

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

inline float timeToCoef (float ms, double sampleRate) noexcept
{
    if (ms <= 0.01f)
        return 1.0f;
    return 1.0f - std::exp (-1.0f / (float) (ms * 0.001 * sampleRate));
}

inline float computeGainDb (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float slope = 1.0f / ratio - 1.0f;
    const float xs = levelDb - thresholdDb;
    const float hw = kneeDb * 0.5f;

    if (xs <= -hw)
        return 0.0f;

    if (xs >= hw)
        return slope * xs;

    if (kneeDb <= 0.0001f)
        return xs > 0.0f ? slope * xs : 0.0f;

    return slope * (xs + hw) * (xs + hw) / (2.0f * kneeDb);
}

class Compressor
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sampleRateHz = sampleRate;
        delayCapacity = (int) std::ceil (0.03 * sampleRate) + 4;

        for (auto& ch : channels)
        {
            ch.delay.assign ((size_t) delayCapacity, 0.0f);
            ch.reset();
        }
    }

    void reset()
    {
        for (auto& ch : channels)
            ch.reset();
    }

    void setParams (const CompressorParams& newParams)
    {
        params = newParams;

        atkCoef  = timeToCoef (params.attackMs, sampleRateHz);
        rmsCoef  = timeToCoef (5.0f, sampleRateHz);
        hpfCoef  = 1.0f - std::exp (-6.2831853f * params.hpfHz / (float) sampleRateHz);
        makeupGain = dbToGain (params.makeupDb);
        delaySamples = juceFreeClamp ((int) std::round (params.lookaheadMs * 0.001 * sampleRateHz), 0, delayCapacity - 1);
    }

    void process (const float* const* mainIn, int numMainChannels,
                  const float* const* sidechainIn, int numSidechainChannels,
                  float* const* out, int numOutChannels, int numSamples)
    {
        blockMinGainDb = 0.0f;

        if (numMainChannels <= 0 || mainIn == nullptr || mainIn[0] == nullptr
            || numOutChannels <= 0 || out == nullptr || numSamples <= 0)
        {
            for (int c = 0; c < numOutChannels; ++c)
                if (out != nullptr && out[c] != nullptr)
                    std::fill_n (out[c], (size_t) numSamples, 0.0f);

            return;
        }

        for (int c = 0; c < numOutChannels; ++c)
        {
            const int state = std::min (c, maxChannels - 1);
            auto& ch = channels[(size_t) state];

            const float* dry   = mainIn[std::min (c, numMainChannels - 1)];
            const float* scSig = nullptr;

            if (sidechainIn != nullptr && numSidechainChannels > 0)
                scSig = sidechainIn[std::min (c, numSidechainChannels - 1)];

            float blockMaxDet = 0.0f;
            float blockMaxOut = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float x = dry[i];
                const float det = scSig != nullptr ? scSig[i] : x;
                const float y = processSample (ch, x, det);

                out[c][i] = y;

                blockMaxDet = std::max (blockMaxDet, std::abs (det));
                blockMaxOut = std::max (blockMaxOut, std::abs (y));
            }

            ch.inputLevelDb  = gainToDb (blockMaxDet);
            ch.outputLevelDb = gainToDb (blockMaxOut);
            blockMinGainDb = std::min (blockMinGainDb, ch.gainDb);
        }
    }

    float getMaxReductionDb() const noexcept  { return -blockMinGainDb; }
    float getGainDb (int channel) const noexcept { return channels[(size_t) std::min (channel, maxChannels - 1)].gainDb; }
    float getInputLevelDb (int channel) const noexcept { return channels[(size_t) std::min (channel, maxChannels - 1)].inputLevelDb; }
    float getOutputLevelDb (int channel) const noexcept { return channels[(size_t) std::min (channel, maxChannels - 1)].outputLevelDb; }
    int   getDelaySamples() const noexcept { return delaySamples; }

private:
    struct ChannelState
    {
        std::vector<float> delay;
        int   writePos = 0;
        float lp1 = 0.0f, lp2 = 0.0f;
        float envSq = 0.0f;
        float gainDb = 0.0f;
        float holdSec = 0.0f;
        float inputLevelDb = -100.0f;
        float outputLevelDb = -100.0f;

        void reset()
        {
            std::fill (delay.begin(), delay.end(), 0.0f);
            writePos = 0;
            lp1 = lp2 = envSq = 0.0f;
            gainDb = 0.0f;
            holdSec = 0.0f;
            inputLevelDb = outputLevelDb = -100.0f;
        }
    };

    static int juceFreeClamp (int v, int lo, int hi) noexcept
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    float processSample (ChannelState& ch, float x, float det)
    {
        float d = det;

        if (params.hpfOn)
        {
            ch.lp1 += hpfCoef * (d - ch.lp1);
            ch.lp2 += hpfCoef * (ch.lp1 - ch.lp2);
            d = ch.lp1 - ch.lp2;
        }

        float levelDb;

        if (params.rmsMode)
        {
            ch.envSq += rmsCoef * (d * d - ch.envSq);
            levelDb = gainToDb (std::sqrt (std::max (ch.envSq, 0.0f)));
        }
        else
        {
            levelDb = gainToDb (std::abs (d));
        }

        float target = computeGainDb (levelDb, params.thresholdDb, params.ratio, params.kneeDb);
        target = std::max (target, -params.rangeDb);

        const bool attacking = target < ch.gainDb - 0.001f;

        if (attacking)
            ch.holdSec = 0.0f;
        else
            ch.holdSec += 1.0f / (float) sampleRateHz;

        float releaseMs = params.releaseMs;

        if (params.autoRelease)
            releaseMs *= 1.0f + std::min (3.0f, ch.holdSec * 2.0f);

        const float coef = attacking ? atkCoef : timeToCoef (releaseMs, sampleRateHz);
        ch.gainDb += (target - ch.gainDb) * coef;

        ch.delay[(size_t) ch.writePos] = x;
        int readPos = ch.writePos - delaySamples;

        if (readPos < 0)
            readPos += delayCapacity;

        const float delayed = ch.delay[(size_t) readPos];
        ch.writePos = (ch.writePos + 1) % delayCapacity;

        const float wet = delayed * dbToGain (ch.gainDb) * makeupGain;
        return delayed + (wet - delayed) * params.mix;
    }

    double sampleRateHz = 48000.0;
    int delayCapacity = 4;
    int delaySamples = 0;
    float atkCoef = 0.0f, rmsCoef = 0.0f, hpfCoef = 0.0f, makeupGain = 1.0f;
    float blockMinGainDb = 0.0f;
    CompressorParams params;
    ChannelState channels[maxChannels];
};

} // namespace kmp
