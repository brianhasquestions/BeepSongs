#pragma once

#include <Windows.h>

/// @file Note.h
/// @brief Defines the Note structure representing a single musical tone.

/// @brief Represents a single musical note defined by its pitch and length.
struct Note
{
    /// @brief Frequency of the note in Hertz. A value of zero represents a rest.
    DWORD Frequency;

    /// @brief Duration of the note in milliseconds.
    DWORD DurationMs;
};
