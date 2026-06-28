#pragma once

#include "MidiTypes.h"
#include <cstdint>
#include <memory>
#include <vector>

/// @file NoteConverter.h
/// @brief Provides functions to convert raw MIDI tick-based notes
///        into millisecond-timed, frequency-based notes suitable
///        for the BeepSongs TimedNote format.

/// @brief A note with millisecond timing and Hz frequency.
struct ConvertedNote
{
    /// @brief Start time in milliseconds from song beginning.
    uint32_t StartMs;

    /// @brief Duration in milliseconds.
    uint32_t DurationMs;

    /// @brief Frequency of the note in Hertz.
    double FrequencyHz;

    /// @brief Volume from 0.0 (silent) to 1.0 (full).
    double Velocity;
};

namespace NoteConverter
{
    /// @brief Builds a unified tempo map from all tracks in a MIDI file.
    /// @param midiFile The parsed MIDI file.
    /// @return Sorted vector of TempoEvent covering the entire piece.
    std::unique_ptr<std::vector<TempoEvent>> BuildTempoMap(const MidiFile& midiFile);

    /// @brief Converts a tick position to milliseconds using the tempo map.
    /// @param tick The absolute tick position.
    /// @param tempoMap Sorted tempo events.
    /// @param ticksPerBeat The file's ticks-per-beat division value.
    /// @return Milliseconds from song start.
    double TickToMs(uint32_t tick, const std::vector<TempoEvent>& tempoMap, uint16_t ticksPerBeat);

    /// @brief Converts a MIDI note number (0-127) to frequency in Hz.
    /// @param noteNumber The MIDI note number (60 = C4, 69 = A4).
    /// @return Frequency in Hertz.
    double MidiNoteToHz(uint8_t noteNumber);

    /// @brief Normalizes a MIDI velocity (0-127) to range [0.0, 1.0].
    /// @param velocity Raw MIDI velocity value.
    /// @return Normalized velocity.
    double NormalizeVelocity(uint8_t velocity);

    /// @brief Converts raw MIDI notes to timed, frequency-based notes.
    /// @param midiNotes The raw MIDI notes from one or more tracks.
    /// @param tempoMap The unified tempo map.
    /// @param ticksPerBeat The file's ticks-per-beat value.
    /// @param velocityScale Multiplier applied to each note's velocity.
    /// @return Heap-allocated vector of ConvertedNote sorted by StartMs.
    std::unique_ptr<std::vector<ConvertedNote>> ConvertNotes(const std::vector<MidiNote>& midiNotes, const std::vector<TempoEvent>& tempoMap, uint16_t ticksPerBeat, double velocityScale);

    /// @brief Applies skyline (monophonic) filtering: at any given time
    ///        only the highest-pitched note is retained.
    /// @param notes The polyphonic note sequence (sorted by StartMs).
    /// @return Heap-allocated filtered monophonic sequence.
    std::unique_ptr<std::vector<ConvertedNote>> ApplySkylineFilter(const std::vector<ConvertedNote>& notes);
}
