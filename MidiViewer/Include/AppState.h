#pragma once

#include <memory>
#include <string>
#include <vector>

#include "MidiTypes.h"
#include "NoteConverter.h"

/// @file AppState.h
/// @brief Central application state shared across the main window,
///        piano-roll renderer, track player, and code exporter.

/// @brief One entry in the pre-computed cache of converted notes,
///        holding the heap-allocated timed sequence for a single
///        MIDI track plus its display color.
struct TrackCacheEntry
{
    /// @brief Heap-allocated sequence of ms-timed notes for this track.
    std::unique_ptr<std::vector<ConvertedNote>> pNotes;

    /// @brief True when the user has the track enabled in the list.
    bool IsSelected;

    /// @brief Packed 0x00BBGGRR color used when drawing this track.
    unsigned int ColorRef;

    /// @brief Lowest MIDI note number used by this track.
    unsigned char LowestNote;

    /// @brief Highest MIDI note number used by this track.
    unsigned char HighestNote;

    /// @brief Linear playback volume multiplier (0.0 to 2.0). Applied
    ///        to each note's velocity when the track is mixed for
    ///        playback. 1.0 means play at the original velocity.
    double Volume;
};

/// @brief Top-level application state owned by the main window and
///        passed by pointer to the child control callbacks.
struct AppState
{
    /// @brief Heap-allocated parsed MIDI file, or null before load.
    std::unique_ptr<MidiFile> pMidiFile;

    /// @brief Heap-allocated unified tempo map for the loaded file.
    std::unique_ptr<std::vector<TempoEvent>> pTempoMap;

    /// @brief Heap-allocated per-track converted-note cache.
    std::unique_ptr<std::vector<TrackCacheEntry>> pTrackCache;

    /// @brief Currently loaded .mid file path (for display / export).
    std::unique_ptr<std::string> pCurrentFilePath;

    /// @brief Total song duration in milliseconds (max end time).
    unsigned int TotalDurationMs;

    /// @brief Lowest note number aggregated across currently-checked
    ///        tracks. Updated whenever a track checkbox toggles.
    unsigned char LowestNote;

    /// @brief Highest note number aggregated across currently-checked
    ///        tracks. Updated whenever a track checkbox toggles.
    unsigned char HighestNote;

    /// @brief Horizontal zoom factor on the piano-roll. 1.0 fits the
    ///        entire song in the client area; larger values zoom in.
    double ZoomX;

    /// @brief Leftmost visible time in milliseconds (scroll position).
    unsigned int ScrollXMs;

    /// @brief Current playback cursor position in milliseconds, or
    ///        0xFFFFFFFF when no track is playing. Drawn as a
    ///        vertical line across the piano-roll.
    unsigned int PlaybackCursorMs;

    /// @brief When true, each checked track is collapsed to its
    ///        top-voice (skyline) before being merged for playback
    ///        and export.
    bool FilterMonophonic;

    /// @brief When true, drops any note whose frequency falls
    ///        outside the audible range (roughly 30 Hz - 16 kHz)
    ///        before playback and export.
    bool FilterClampAudible;

    /// @brief Number of octaves to transpose every note. Positive
    ///        shifts up, negative shifts down. Clamped to a small
    ///        range so synthesis never exceeds Nyquist.
    int OctaveShift;
};
