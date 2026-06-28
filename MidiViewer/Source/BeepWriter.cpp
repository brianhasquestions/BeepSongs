#include "BeepWriter.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "BeepFormat.h"
#include "TimedNote.h"

namespace
{
    uint32_t ComputeTotalDurationMs(const std::vector<ConvertedNote>& notes)
    {
        uint32_t maxEndMs = 0;
        for (const ConvertedNote& note : notes)
        {
            const uint32_t endMs = note.StartMs + note.DurationMs;
            if (endMs > maxEndMs)
            {
                maxEndMs = endMs;
            }
        }
        return maxEndMs;
    }

    void FillHeader(BeepFormat::FileHeader& header, const std::vector<ConvertedNote>& notes, const BeepWriter::SongInfo& info)
    {
        std::memset(&header, 0, sizeof(header));
        header.Magic = BeepFormat::Magic;
        header.Version = BeepFormat::CurrentVersion;
        header.NoteCount = static_cast<DWORD>(notes.size());
        header.TotalDurationMs = ComputeTotalDurationMs(notes);
        header.NotesOffset = static_cast<DWORD>(sizeof(BeepFormat::FileHeader));
        header.NoteStride = static_cast<DWORD>(sizeof(TimedNote));
        header.OriginalBpm = info.BpmEstimate;

        // Copy the display name without strncpy so the build stays clean
        // under /W4 /WX with SDL checks; always leave a null terminator.
        const size_t copyLength = (std::min)(info.DisplayName.size(), BeepFormat::MaxNameLength - 1);
        std::memcpy(header.Name, info.DisplayName.data(), copyLength);
        header.Name[copyLength] = '\0';
    }
}

namespace BeepWriter
{
    bool Write(const std::vector<ConvertedNote>& notes, const SongInfo& info, const std::string& filePath)
    {
        BeepFormat::FileHeader header;
        FillHeader(header, notes, info);

        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (false == out.is_open())
        {
            return false;
        }

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // Each ConvertedNote is copied into a TimedNote so the bytes on disk
        // exactly match the in-memory layout the runtime maps and reads.
        for (const ConvertedNote& note : notes)
        {
            TimedNote record;
            record.StartMs = note.StartMs;
            record.DurationMs = note.DurationMs;
            record.FrequencyHz = note.FrequencyHz;
            record.Velocity = note.Velocity;
            out.write(reinterpret_cast<const char*>(&record), sizeof(record));
        }

        return out.good();
    }
}
