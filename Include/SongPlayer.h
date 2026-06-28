#pragma once

#include <Windows.h>
#include <vector>
#include "Note.h"
#include "TimedNote.h"

/// @file SongPlayer.h
/// @brief Provides functions to play monophonic note sequences and
///        polyphonic timed note sequences using waveOutOpen() with
///        sine wave synthesis.

namespace SongPlayer
{
    /// @brief Returns the console Ctrl+C handler function pointer.
    ///        Install once at program startup with SetConsoleCtrlHandler
    ///        so Ctrl+C stops playback without killing the process.
    PHANDLER_ROUTINE GetCtrlHandler();

    /// @brief Plays a monophonic song by synthesizing sequential notes
    ///        into a PCM buffer and submitting it to the wave device.
    /// @param notes The sequence of notes that compose the song.
    void PlaySong(const std::vector<Note>& notes);

    /// @brief Plays a polyphonic song by synthesizing timed notes
    ///        into a PCM buffer. Multiple notes that overlap in time
    ///        are summed together to produce chords and harmonies.
    /// @param timedNotes The collection of timed notes with absolute
    ///        start positions, allowing simultaneous playback.
    void PlayTimedSong(const std::vector<TimedNote>& timedNotes);

    /// @brief Plays a polyphonic song directly from a contiguous note
    ///        array, such as a memory-mapped .beep file, without copying
    ///        the notes into a vector first.
    /// @param notes Pointer to the first of count timed notes.
    /// @param count Number of notes the pointer addresses.
    void PlayTimedSong(const TimedNote* notes, size_t count);
}
