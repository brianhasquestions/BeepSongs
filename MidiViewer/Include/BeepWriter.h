#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "NoteConverter.h"

/// @file BeepWriter.h
/// @brief Serializes converted notes into a BeepSong (.beep) binary file
///        using the shared BeepFormat layout. The resulting file can be
///        dropped straight into the BeepSongs project's Songs folder and
///        is memory-mapped at playback time with no parsing.

namespace BeepWriter
{
    /// @brief Metadata written into the .beep file header.
    struct SongInfo
    {
        /// @brief Display name shown in the BeepSongs menu.
        std::string DisplayName;

        /// @brief Source tempo in beats per minute, or zero if unknown.
        uint32_t BpmEstimate;
    };

    /// @brief Writes the notes and metadata to the given path, overwriting
    ///        any existing file. The path should include the .beep extension.
    /// @param notes The converted notes to serialize, in any order.
    /// @param info Header metadata describing the song.
    /// @param filePath Destination file path.
    /// @return True if the file was written in full, false otherwise.
    bool Write(const std::vector<ConvertedNote>& notes, const SongInfo& info, const std::string& filePath);
}
