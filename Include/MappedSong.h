#pragma once

#include <Windows.h>
#include <string>
#include "TimedNote.h"

/// @file MappedSong.h
/// @brief Memory-maps a BeepSong (.beep) file so its note array can be read
///        straight from the mapped view with no parsing and no copying. The
///        object owns the file and mapping handles and releases them when it
///        is destroyed.

class MappedSong
{
public:
    /// @brief Maps and validates the .beep file at the given path. When the
    ///        file is missing, malformed, or built for a newer format, the
    ///        object is left invalid; check IsValid() before using it.
    explicit MappedSong(const std::wstring& filePath);

    /// @brief Unmaps the view and closes the file and mapping handles.
    ~MappedSong();

    MappedSong(const MappedSong&) = delete;
    MappedSong& operator=(const MappedSong&) = delete;

    /// @brief Transfers ownership of the mapping; the moved-from song
    ///        becomes invalid.
    MappedSong(MappedSong&& other) noexcept;
    MappedSong& operator=(MappedSong&& other) noexcept;

    /// @brief True when the file mapped and its header passed validation.
    bool IsValid() const;

    /// @brief Display name taken from the file header.
    const std::string& Name() const;

    /// @brief Pointer to the first note in the mapped view, valid for
    ///        NoteCount() records. Returns nullptr when the song is invalid.
    const TimedNote* Notes() const;

    /// @brief Number of notes reachable through Notes().
    size_t NoteCount() const;

private:
    /// @brief Releases the view and handles and resets every member to the
    ///        empty, invalid state. Safe to call on an already-empty object.
    void Reset();

    HANDLE fileHandle;
    HANDLE mappingHandle;
    const void* viewBase;
    const TimedNote* notes;
    size_t noteCount;
    std::string name;
};
