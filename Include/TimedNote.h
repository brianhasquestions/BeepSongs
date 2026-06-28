#pragma once

#include <Windows.h>

/// @file TimedNote.h
/// @brief Defines the TimedNote structure for polyphonic playback
///        where each note has an absolute start time in the song.

struct TimedNote
{
    /// @brief Start time of the note in milliseconds from song beginning.
    DWORD StartMs;

    /// @brief Duration of the note in milliseconds.
    DWORD DurationMs;

    /// @brief Frequency of the note in Hertz (as a double for precision).
    double FrequencyHz;

    /// @brief Volume of the note from 0.0 (silent) to 1.0 (full).
    double Velocity;
};
