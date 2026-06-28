#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>

/// @file ExportDialog.h
/// @brief Declares a programmatic modal dialog that prompts the user
///        for the metadata needed to install a generated song into
///        the sibling BeepSongs project.

namespace ExportDialog
{
    /// @brief Fields collected by the export dialog.
    struct ExportInfo
    {
        /// @brief File-stem / identifier base (e.g. "InuyashaTheme").
        ///        The generated function is named Get<BaseName>, the
        ///        files are <BaseName>.h and <BaseName>.cpp, and the
        ///        menu enum constant is <BaseName>.
        std::string BaseName;

        /// @brief Human-readable display name for menus and doc
        ///        comments (e.g. "Inuyasha Main Theme").
        std::string DisplayName;

        /// @brief Free-form description for doc comments
        ///        (e.g. "from Inuyasha").
        std::string Description;

        /// @brief Estimated tempo in beats per minute.
        uint32_t BpmEstimate;
    };

    /// @brief Shows the modal export dialog and blocks until the user
    ///        clicks OK or Cancel.
    /// @param hOwner Parent window that gets disabled during the modal.
    /// @param pOutInfo Destination for the collected fields.
    /// @return True on OK, false on Cancel or close.
    bool Show(HWND hOwner, ExportInfo* pOutInfo);
}
