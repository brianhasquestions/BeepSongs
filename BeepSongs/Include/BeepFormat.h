#pragma once

#include <Windows.h>
#include "TimedNote.h"

/// @file BeepFormat.h
/// @brief Defines the on-disk layout of a BeepSong (.beep) file. The note
///        array stored on disk is a byte-for-byte copy of an array of
///        TimedNote records, so a memory-mapped file can be read directly
///        with no parsing or copying. This header is shared by the tool
///        that writes .beep files and the runtime that maps them.

namespace BeepFormat
{
    /// @brief File signature 'BEEP' stored little-endian as the first DWORD.
    constexpr DWORD Magic = 0x50454542;

    /// @brief Current format version written by the exporter.
    constexpr DWORD CurrentVersion = 1;

    /// @brief Maximum length, including the null terminator, of the song
    ///        name stored in the header.
    constexpr size_t MaxNameLength = 64;

    /// @brief Sentinel for FileHeader::OriginalBpm meaning the source tempo
    ///        is unknown (the songs use absolute millisecond timing).
    constexpr DWORD UnknownBpm = 0;

    /// @brief Byte boundary the note array must start on so the doubles in
    ///        each TimedNote stay naturally aligned in the mapped view.
    constexpr DWORD NoteArrayAlignment = 8;

    /// @brief Fixed-size header that precedes the note array in every .beep
    ///        file. Its size is a multiple of eight so the note array that
    ///        follows starts on an eight-byte boundary, keeping the doubles
    ///        inside each TimedNote naturally aligned in the mapped view.
    struct FileHeader
    {
        /// @brief Must equal Magic; identifies the file as a BeepSong file.
        DWORD Magic;

        /// @brief Format version; the runtime rejects versions it post-dates.
        DWORD Version;

        /// @brief Number of TimedNote records stored after the header.
        DWORD NoteCount;

        /// @brief Total song length in milliseconds, for display and for
        ///        sizing the playback buffer without scanning the notes.
        DWORD TotalDurationMs;

        /// @brief Byte offset from the start of the file to the note array.
        DWORD NotesOffset;

        /// @brief Size of a single on-disk note record. Stored so the runtime
        ///        can reject a file whose layout does not match TimedNote.
        DWORD NoteStride;

        /// @brief Source tempo in beats per minute, or UnknownBpm if unknown.
        DWORD OriginalBpm;

        /// @brief Reserved; keeps the header eight-byte aligned and leaves
        ///        room for future fields without a version bump.
        DWORD Reserved;

        /// @brief Display name, null-terminated and null-padded.
        char Name[MaxNameLength];
    };

    static_assert(24 == sizeof(TimedNote),
        "TimedNote must stay 24 bytes so the on-disk note layout matches memory");

    static_assert(0 == sizeof(FileHeader) % NoteArrayAlignment,
        "FileHeader size must be a multiple of eight to keep the note array aligned");
}
