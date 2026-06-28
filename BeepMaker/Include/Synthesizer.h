#pragma once

#include <Windows.h>
#include <cstdint>
#include "Pattern.h"

/// @file Synthesizer.h
/// @brief Provides waveform generation (sine, square, saw, triangle)
///        with modulation effects and synthesized drum sounds.

class Synthesizer
{
public:
    Synthesizer();
    double GenerateWaveformSample(WaveformType waveform, double phase) const;
    double GenerateModulatedSample(WaveformType waveform, double baseFreqHz, uint32_t modulation, double phase, DWORD sampleOffset) const;
    double GenerateDrumSample(DrumType drum, DWORD sampleOffset);
    static DWORD GetDrumDurationSamples(DrumType drum);
    double ApplyAdsr(double sample, DWORD sampleOffset, DWORD totalDuration) const;

private:
    uint32_t NoiseState;
    double GenerateNoiseSample();
};
