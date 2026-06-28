#pragma once

#include <memory>
#include <vector>

#include "AppState.h"
#include "NoteConverter.h"

/// @file NoteFilters.h
/// @brief Central point for turning the per-track cache into a
///        single merged and filtered note stream, honoring the
///        application's filter state and (optionally) each track's
///        playback volume.

namespace NoteFilters
{
    /// @brief Collects notes from every checked track, applies the
    ///        skyline filter per-track when requested, transposes,
    ///        clamps to audible range, optionally scales by each
    ///        track's playback volume, and sorts the result.
    /// @param pState Application state with the track cache and
    ///        current filter flags.
    /// @param applyVolume When true, each note's velocity is
    ///        multiplied by its track's Volume before being added
    ///        to the merged stream. Pass false when exporting code
    ///        that should represent the source material.
    /// @return Heap-allocated vector sorted by StartMs, FrequencyHz.
    std::unique_ptr<std::vector<ConvertedNote>> BuildMergedNotes(const AppState* pState, bool applyVolume);
}
