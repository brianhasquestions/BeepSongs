#pragma once

#include <Windows.h>
#include <mmsystem.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#pragma comment(lib, "winmm.lib")

#include "Pattern.h"
#include "Synthesizer.h"

/// @file AudioEngine.h
/// @brief Provides real-time double-buffered audio playback using
///        the Windows waveOut API with CALLBACK_FUNCTION for
///        continuous loop playback of the step sequencer pattern.

namespace AudioEngineConstants
{
    constexpr DWORD BufferCount = 3;
    constexpr DWORD BufferDurationMs = 50;
    constexpr DWORD SamplesPerBuffer = (Audio::SampleRate * BufferDurationMs) / 1000;
    constexpr int QueueCapacity = 64;
}

/// @brief Describes a one-shot sound to be played immediately.
struct OneShotSound
{
    InstrumentType Type;
    WaveformType Waveform;
    DrumType Drum;
    double FrequencyHz;
    double Volume;
    uint32_t Modulation;
    DWORD DurationSamples;
};

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();
    bool Start(Pattern* pPattern);
    void Stop();
    bool IsRunning() const;
    /// @brief Queues a one-shot sound. Multiple calls chain sequentially.
    void QueueSound(const OneShotSound& sound);

private:
    static void CALLBACK WaveOutCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    void FillBuffer(WAVEHDR* pHeader);

    HWAVEOUT WaveOut;
    Pattern* pActivePattern;
    Synthesizer Synth;
    std::atomic<bool> Running;
    std::array<WAVEHDR, AudioEngineConstants::BufferCount> WaveHeaders;
    std::array<std::unique_ptr<short[]>, AudioEngineConstants::BufferCount> Buffers;
    std::unique_ptr<std::vector<double>> pPhases;
    std::unique_ptr<std::vector<DWORD>> pDrumOffsets;
    std::unique_ptr<std::vector<bool>> pDrumActive;
    DWORD SamplePositionInStep;
    int PlaybackStep;
    std::array<OneShotSound, AudioEngineConstants::QueueCapacity> SoundQueue;
    std::atomic<int> QueueWritePos;
    int QueueReadPos;
    bool OneShotPlaying;
    DWORD OneShotOffset;
    double OneShotPhase;
    OneShotSound ActiveOneShot;
};
