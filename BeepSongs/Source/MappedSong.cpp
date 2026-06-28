#include <cstdint>
#include <cstring>
#include <utility>

#include "MappedSong.h"
#include "BeepFormat.h"

// The constructor performs all validation up front so the rest of the
// program can treat a valid MappedSong as a trusted, ready-to-read view.
// 'notes' doubling as the validity flag keeps IsValid() a single test.

MappedSong::MappedSong(const std::wstring& filePath)
    : fileHandle(INVALID_HANDLE_VALUE)
    , mappingHandle(nullptr)
    , viewBase(nullptr)
    , notes(nullptr)
    , noteCount(0)
{
    fileHandle = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (INVALID_HANDLE_VALUE == fileHandle)
    {
        Reset();
        return;
    }

    LARGE_INTEGER fileSize = {};
    if (FALSE == GetFileSizeEx(fileHandle, &fileSize))
    {
        Reset();
        return;
    }

    if (static_cast<LONGLONG>(sizeof(BeepFormat::FileHeader)) > fileSize.QuadPart)
    {
        Reset();
        return;
    }

    mappingHandle = CreateFileMappingW(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (nullptr == mappingHandle)
    {
        Reset();
        return;
    }

    viewBase = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
    if (nullptr == viewBase)
    {
        Reset();
        return;
    }

    const BeepFormat::FileHeader* header =
        reinterpret_cast<const BeepFormat::FileHeader*>(viewBase);

    // Reject anything that is not a current-or-older .beep whose note records
    // match this build's TimedNote layout; otherwise the cast below is unsafe.
    const bool headerOk =
        (BeepFormat::Magic == header->Magic) &&
        (BeepFormat::CurrentVersion >= header->Version) &&
        (sizeof(TimedNote) == header->NoteStride) &&
        (sizeof(BeepFormat::FileHeader) <= header->NotesOffset) &&
        (0 == header->NotesOffset % BeepFormat::NoteArrayAlignment);
    if (false == headerOk)
    {
        Reset();
        return;
    }

    // Confirm the note array actually fits inside the file before trusting it.
    const uint64_t notesBytes =
        static_cast<uint64_t>(header->NoteCount) * sizeof(TimedNote);
    const uint64_t requiredBytes = static_cast<uint64_t>(header->NotesOffset) + notesBytes;
    if (requiredBytes > static_cast<uint64_t>(fileSize.QuadPart))
    {
        Reset();
        return;
    }

    const size_t nameLength = ::strnlen(header->Name, BeepFormat::MaxNameLength);
    name.assign(header->Name, nameLength);

    const BYTE* base = static_cast<const BYTE*>(viewBase);
    notes = reinterpret_cast<const TimedNote*>(base + header->NotesOffset);
    noteCount = header->NoteCount;
}

MappedSong::~MappedSong()
{
    Reset();
}

MappedSong::MappedSong(MappedSong&& other) noexcept
    : fileHandle(other.fileHandle)
    , mappingHandle(other.mappingHandle)
    , viewBase(other.viewBase)
    , notes(other.notes)
    , noteCount(other.noteCount)
    , name(std::move(other.name))
{
    other.fileHandle = INVALID_HANDLE_VALUE;
    other.mappingHandle = nullptr;
    other.viewBase = nullptr;
    other.notes = nullptr;
    other.noteCount = 0;
}

MappedSong& MappedSong::operator=(MappedSong&& other) noexcept
{
    if (this != &other)
    {
        Reset();

        fileHandle = other.fileHandle;
        mappingHandle = other.mappingHandle;
        viewBase = other.viewBase;
        notes = other.notes;
        noteCount = other.noteCount;
        name = std::move(other.name);

        other.fileHandle = INVALID_HANDLE_VALUE;
        other.mappingHandle = nullptr;
        other.viewBase = nullptr;
        other.notes = nullptr;
        other.noteCount = 0;
    }
    return *this;
}

bool MappedSong::IsValid() const
{
    return nullptr != notes;
}

const std::string& MappedSong::Name() const
{
    return name;
}

const TimedNote* MappedSong::Notes() const
{
    return notes;
}

size_t MappedSong::NoteCount() const
{
    return noteCount;
}

void MappedSong::Reset()
{
    if (nullptr != viewBase)
    {
        UnmapViewOfFile(viewBase);
    }
    if (nullptr != mappingHandle)
    {
        CloseHandle(mappingHandle);
    }
    if (INVALID_HANDLE_VALUE != fileHandle)
    {
        CloseHandle(fileHandle);
    }

    fileHandle = INVALID_HANDLE_VALUE;
    mappingHandle = nullptr;
    viewBase = nullptr;
    notes = nullptr;
    noteCount = 0;
    name.clear();
}
