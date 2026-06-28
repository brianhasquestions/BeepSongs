#pragma once

#include <memory>
#include <string>

#include "AppState.h"
#include "ExportDialog.h"

/// @file BeepSongsInstaller.h
/// @brief Writes a generated song to the sibling BeepSongs project's
///        Include\\Songs and Source\\Songs directories, and patches
///        the BeepSongs project files and Main.cpp menu listing so
///        the new song is picked up by the next build.

namespace BeepSongsInstaller
{
    /// @brief Walks up from the running executable's directory to
    ///        locate a folder containing BeepSongs.vcxproj.
    /// @return Heap-allocated absolute path with no trailing slash,
    ///         or an empty string if the file was not found.
    std::unique_ptr<std::string> FindBeepSongsRoot();

    /// @brief Installs the song described by info into BeepSongs:
    ///        writes the .h/.cpp, patches BeepSongs.vcxproj, patches
    ///        BeepSongs.vcxproj.filters, and patches BeepSongs's
    ///        Source\\Main.cpp menu listing.
    /// @param pState Application state providing the selected notes.
    /// @param info Export metadata gathered from the export dialog.
    /// @return Heap-allocated human-readable status string. On
    ///         success the string starts with "OK".
    std::unique_ptr<std::string> Install(const AppState* pState, const ExportDialog::ExportInfo& info);
}
