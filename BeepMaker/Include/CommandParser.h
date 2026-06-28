#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Pattern.h"

/// @file CommandParser.h
/// @brief Parses REPL commands for the live-coding beat maker.

enum class CommandType { PlayOnce, Loop, Stop, StopAll, SetBpm, GetBpm, Bind, Binds, Help, Clear, Exit, Invalid };

struct ParsedCommand
{
    CommandType Type;
    InstrumentType Instrument;
    WaveformType Waveform;
    DrumType Drum;
    double FrequencyHz;
    uint32_t Modulation;
    int DurationSteps;
    std::unique_ptr<std::vector<int>> pSteps;
    int BpmValue;
    int BindSlot;
    std::string BindCommand;
    std::string InstrumentKey;
    std::string ErrorMessage;

    ParsedCommand()
        : Type(CommandType::Invalid), Instrument(InstrumentType::Drum), Waveform(WaveformType::Sine),
          Drum(DrumType::Kick), FrequencyHz(0.0), Modulation(Mod::None), DurationSteps(1),
          pSteps(std::make_unique<std::vector<int>>()), BpmValue(0), BindSlot(0)
    {
    }
};

namespace CommandParser
{
    ParsedCommand Parse(const std::string& input);
}
