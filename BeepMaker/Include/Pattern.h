#pragma once

#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @file Pattern.h
/// @brief Defines the step sequencer pattern grid with instrument
///        rows, step toggles, and BPM control.

namespace Audio
{
    constexpr DWORD SampleRate = 44100;
    constexpr WORD BitsPerSample = 16;
    constexpr WORD NumChannels = 1;
    constexpr double Pi = 3.14159265358979323846;
    constexpr double MaxAmplitude = 30000.0;
    constexpr double PeakClamp = 32000.0;
}

namespace Sequencer
{
    constexpr int StepCount = 16;
    constexpr int DefaultBpm = 120;
    constexpr int MinBpm = 60;
    constexpr int MaxBpm = 240;
    constexpr int BpmStep = 5;
    constexpr int StepsPerBeat = 4;
    constexpr int SecondsPerMinute = 60;
    constexpr int MaxRows = 64;
}

enum class InstrumentType { Synth, Drum };
enum class WaveformType { Sine, Square, Sawtooth, Triangle, Sub, Wub, Chip };
enum class DrumType { Kick, Snare, ClosedHat, OpenHat, Clap, Kick808, Snare808, Hat808, OpenHat808, Clap808, Ride, Rim, Tom };

/// @brief Flags for synth modulation effects (combinable via bitwise OR).
namespace Mod
{
    constexpr uint32_t None       = 0x00;
    constexpr uint32_t Detune     = 0x01;
    constexpr uint32_t Vibrato    = 0x02;
    constexpr uint32_t Tremolo    = 0x04;
    constexpr uint32_t Pwm        = 0x08;
    constexpr uint32_t Distortion = 0x10;
    constexpr uint32_t Adsr       = 0x20;
    constexpr uint32_t Bitcrush   = 0x40;
}

/// @brief ADSR envelope parameters.
namespace Envelope
{
    constexpr double AttackMs  = 20.0;
    constexpr double DecayMs   = 80.0;
    constexpr double SustainLevel = 0.7;
    constexpr double ReleaseMs = 50.0;
}

/// @brief Modulation LFO and effect parameters.
namespace ModParams
{
    constexpr double VibratoRateHz = 5.0;
    constexpr double VibratoDepthSemitones = 0.3;
    constexpr double TremoloRateHz = 4.0;
    constexpr double TremoloDepth = 0.4;
    constexpr double DetuneAmountCents = 12.0;
    constexpr int DetuneVoices = 3;
    constexpr double PwmRateHz = 1.5;
    constexpr double PwmMinWidth = 0.1;
    constexpr double PwmMaxWidth = 0.9;
    constexpr double DistortionGain = 3.0;
    constexpr double DistortionClip = 0.8;
    constexpr double BitcrushLevels = 12.0;
}

struct InstrumentRow
{
    std::string Name;
    InstrumentType Type;
    WaveformType Waveform;
    DrumType Drum;
    double FrequencyHz;
    double Volume;
    uint32_t Modulation;
    std::array<bool, Sequencer::StepCount> Steps;
};

class Pattern
{
public:
    Pattern();
    void ToggleStep(int row, int column);
    bool GetStep(int row, int column) const;
    int GetRowCount() const;
    const InstrumentRow& GetRow(int index) const;
    void SetLoop(const std::string& key, InstrumentType type, WaveformType waveform, DrumType drum, double freqHz, uint32_t modulation, const std::vector<int>& steps);
    void ClearLoop(const std::string& key);
    void ClearAllLoops();
    std::unique_ptr<std::vector<std::pair<std::string, std::array<bool, Sequencer::StepCount>>>> GetActiveLoops() const;
    int GetBpm() const;
    void SetBpm(int bpm);
    DWORD GetStepDurationSamples() const;
    std::atomic<int> CurrentStep;

private:
    std::unique_ptr<std::vector<InstrumentRow>> pRows;
    std::atomic<int> Bpm;
};
