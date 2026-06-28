#pragma once

#include "MidiTypes.h"
#include <memory>
#include <string>
#include <vector>

/// @file MidiParser.h
/// @brief Provides functions to parse Standard MIDI Files from disk
///        into structured MidiFile data.

namespace MidiParser
{
    /// @brief Parses a Standard MIDI File from disk into structured data.
    /// @param filePath Absolute or relative path to the .mid file.
    /// @return A heap-allocated MidiFile structure.
    std::unique_ptr<MidiFile> Parse(const std::string& filePath);

    /// @brief Builds summary information for each track in a parsed file.
    /// @param midiFile The parsed MIDI file to summarize.
    /// @return A heap-allocated vector of TrackInfo, one per track.
    std::unique_ptr<std::vector<TrackInfo>> GetTrackSummaries(const MidiFile& midiFile);
}
