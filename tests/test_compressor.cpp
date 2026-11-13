#include "../Source/DSP/CompressorCore.h"

#include <cmath>
#include <cstdio>
#include <random>

static int failures = 0;

static void expectNear (const char* name, float actual, float expected, float tol)
{
    const bool ok = std::fabs (actual - expected) <= tol;
    std::printf ("[%s] %-38s actual=%9.4f expected=%9.4f tol=%.4f\n",
                 ok ? "PASS" : "FAIL", name, actual, expected, tol);

    if (! ok)
        ++failures;
}

static float measureOutputDb (kmp::Compressor& comp, float amp, int numSamples, int settle)
{
    std::vector<float> in (numSamples), out (numSamples);
    const double sr = 48000.0;

    for (int i = 0; i < numSamples; ++i)
        in[(size_t) i] = amp * std::sin (2.0 * 3.14159265358979 * 1000.0 * i / sr);

    comp.reset();

    const float* inPtrs[1] = { in.data() };
    float* outPtrs[1] = { out.data() };

    comp.process (inPtrs, 1, nullptr, 0, outPtrs, 1, numSamples);

    double sumSq = 0.0;

    for (int i = settle; i < numSamples; ++i)
        sumSq += out[(size_t) i] * out[(size_t) i];

    const float rms = (float) std::sqrt (sumSq / (numSamples - settle));
    return kmp::gainToDb (rms);
}

int main()
{
    kmp::CompressorParams p;
    p.thresholdDb = -30.0f;
    p.ratio = 4.0f;
    p.attackMs = 1.0f;
    p.releaseMs = 50.0f;
    p.kneeDb = 0.0f;
    p.rangeDb = 60.0f;
    p.makeupDb = 0.0f;
    p.mix = 1.0f;
    p.lookaheadMs = 0.0f;

    kmp::Compressor comp;
    comp.prepare (48000.0, 512);

    // 1) steady-state gain reduction: -10 dB peak sine over -30 dB threshold, 4:1 hard knee
    //    ideal output peak = -30 + 20/4 = -25 dB -> RMS = -28.01 dB (peak detector recovers
    //    slightly between cycles, hence the tolerance)
    comp.setParams (p);
    const float outDb = measureOutputDb (comp, kmp::dbToGain (-10.0f), 96000, 48000);
    expectNear ("steady state 4:1 hard knee", outDb, -27.9f, 0.8f);

    // 2) below threshold: unity (within meter tolerance)
    p.thresholdDb = -5.0f;
    comp.setParams (p);
    const float outDb2 = measureOutputDb (comp, kmp::dbToGain (-10.0f), 48000, 24000);
    expectNear ("below threshold unity", outDb2, -13.01f, 0.3f);

    // 3) bypass via mix = 0
    p.thresholdDb = -30.0f;
    p.mix = 0.0f;
    comp.setParams (p);
    const float outDb3 = measureOutputDb (comp, kmp::dbToGain (-10.0f), 48000, 24000);
    expectNear ("mix 0 passthrough", outDb3, -13.01f, 0.3f);

    // 4) makeup gain
    p.mix = 1.0f;
    p.makeupDb = 6.0f;
    comp.setParams (p);
    const float outDb4 = measureOutputDb (comp, kmp::dbToGain (-10.0f), 96000, 48000);
    expectNear ("makeup +6 dB", outDb4, -27.9f + 6.0f, 0.8f);
    p.makeupDb = 0.0f;

    // 5) range limit: range 6 dB caps reduction at 6 dB
    p.rangeDb = 6.0f;
    comp.setParams (p);
    const float outDb5 = measureOutputDb (comp, kmp::dbToGain (-10.0f), 96000, 48000);
    expectNear ("range caps at 6 dB", outDb5, -19.0f, 0.5f);
    p.rangeDb = 60.0f;

    // 6) gain computer curve: soft knee continuous, endpoints match hard knee
    const float gLow  = kmp::computeGainDb (-40.0f, -30.0f, 4.0f, 12.0f);
    const float gHigh = kmp::computeGainDb (0.0f, -30.0f, 4.0f, 12.0f);
    const float gMid  = kmp::computeGainDb (-30.0f, -30.0f, 4.0f, 12.0f);
    expectNear ("knee below window = 0 dB", gLow, 0.0f, 0.0001f);
    expectNear ("knee above matches slope", gHigh, (1.0f / 4.0f - 1.0f) * 30.0f, 0.0001f);
    expectNear ("knee centre = slope*knee/8", gMid, (1.0f / 4.0f - 1.0f) * 12.0f / 8.0f, 0.0001f);

    // 7) attack ballistics: reduction must build over time, not instantly
    p.attackMs = 50.0f;
    comp.setParams (p);
    {
        std::vector<float> in (9600), out (9600);

        for (int i = 0; i < 9600; ++i)
            in[(size_t) i] = kmp::dbToGain (-10.0f) * ((i % 2) == 0 ? 1.0f : -1.0f);

        comp.reset();
        const float* inPtrs[1] = { in.data() };
        float* outPtrs[1] = { out.data() };
        comp.process (inPtrs, 1, nullptr, 0, outPtrs, 1, 480);
        expectNear ("slow attack keeps GR small early", std::fabs (comp.getGainDb (0)), 2.7f, 1.2f);

        comp.process (inPtrs, 1, nullptr, 0, outPtrs, 1, 9120);
        expectNear ("GR settles toward target", comp.getMaxReductionDb(), 15.0f, 1.5f);
    }

    // 8) lookahead delay reports correct latency
    p.attackMs = 10.0f;
    p.lookaheadMs = 5.0f;
    comp.setParams (p);
    expectNear ("lookahead latency samples", (float) comp.getDelaySamples(), 240.0f, 1.0f);

    // 9) silence in -> silence out, no denormal blowup
    p.lookaheadMs = 0.0f;
    comp.setParams (p);
    {
        std::vector<float> in (4800, 0.0f), out (4800, 0.0f);
        comp.reset();
        const float* inPtrs[1] = { in.data() };
        float* outPtrs[1] = { out.data() };
        comp.process (inPtrs, 1, nullptr, 0, outPtrs, 1, 4800);
        float maxAbs = 0.0f;

        for (float v : out)
            maxAbs = std::max (maxAbs, std::fabs (v));

        expectNear ("silence stays silent", maxAbs, 0.0f, 0.0f);
    }

    // 10) external sidechain: quiet tone ducked by loud sidechain noise
    {
        std::vector<float> in (96000), sc (96000), out (96000);
        std::mt19937 rng (42);
        std::uniform_real_distribution<float> dist (-0.5f, 0.5f);

        for (int i = 0; i < 96000; ++i)
        {
            sc[(size_t) i] = dist (rng);
            in[(size_t) i] = kmp::dbToGain (-40.0f) * std::sin (2.0 * 3.14159265358979 * 1000.0 * i / 48000.0);
        }

        comp.reset();
        const float* inPtrs[1] = { in.data() };
        const float* scPtrs[1] = { sc.data() };
        float* outPtrs[1] = { out.data() };
        comp.process (inPtrs, 1, scPtrs, 1, outPtrs, 1, 96000);

        double sumSq = 0.0;

        for (int i = 48000; i < 96000; ++i)
            sumSq += out[(size_t) i] * out[(size_t) i];

        const float rmsDb = kmp::gainToDb ((float) std::sqrt (sumSq / 48000.0));
        expectNear ("external SC ducks quiet input", rmsDb, -57.5f, 2.0f);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
