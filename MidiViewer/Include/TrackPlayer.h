#pragma once

#include <Windows.h>
#include <vector>

#include "NoteConverter.h"

/// @file TrackPlayer.h
/// @brief Background-thread playback of a note sequence using the Win32
///        waveOut PCM API. Overlapping notes are summed into a single
///        mix buffer and peak-normalized. Playback is interruptible via
///        a stop event that the UI signals when the user clicks stop.

namespace TrackPlayer
{
    /// @brief Starts playback of the given notes on a background thread,
    ///        synthesizing them into a single PCM mix buffer.
    /// @param pNotes Heap-allocated converted-note sequence to play.
    ///        Ownership is transferred to the player for the run.
    /// @return True if the thread was started, false if busy.
    bool Start(std::unique_ptr<std::vector<ConvertedNote>> pNotes);

    /// @brief Signals the running thread to stop and waits for it.
    void Stop();

    /// @brief Returns true while a playback thread is active.
    bool IsPlaying();

    /// @brief Returns milliseconds elapsed since the most recent
    ///        successful Start() call, or 0 if no playback is active.
    unsigned int GetElapsedMs();
}
