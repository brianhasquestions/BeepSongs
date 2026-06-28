#include "MidiParser.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr uint32_t GmProgramCount = 128;
    constexpr uint32_t TrackHeaderSize = 8;
    constexpr int SmpteNtscFpsCode = 29;
    constexpr double RoundingBias = 0.5;

    constexpr const char* GeneralMidiPrograms[GmProgramCount] =
    {
        "Acoustic Grand Piano", "Bright Acoustic Piano",
        "Electric Grand Piano", "Honky-tonk Piano",
        "Electric Piano 1", "Electric Piano 2",
        "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel",
        "Music Box", "Vibraphone",
        "Marimba", "Xylophone",
        "Tubular Bells", "Dulcimer",
        "Drawbar Organ", "Percussive Organ",
        "Rock Organ", "Church Organ",
        "Reed Organ", "Accordion",
        "Harmonica", "Tango Accordion",
        "Nylon Guitar", "Steel Guitar",
        "Jazz Guitar", "Clean Guitar",
        "Muted Guitar", "Overdriven Guitar",
        "Distortion Guitar", "Guitar Harmonics",
        "Acoustic Bass", "Electric Bass (finger)",
        "Electric Bass (pick)", "Fretless Bass",
        "Slap Bass 1", "Slap Bass 2",
        "Synth Bass 1", "Synth Bass 2",
        "Violin", "Viola",
        "Cello", "Contrabass",
        "Tremolo Strings", "Pizzicato Strings",
        "Orchestral Harp", "Timpani",
        "String Ensemble 1", "String Ensemble 2",
        "Synth Strings 1", "Synth Strings 2",
        "Choir Aahs", "Voice Oohs",
        "Synth Voice", "Orchestra Hit",
        "Trumpet", "Trombone",
        "Tuba", "Muted Trumpet",
        "French Horn", "Brass Section",
        "Synth Brass 1", "Synth Brass 2",
        "Soprano Sax", "Alto Sax",
        "Tenor Sax", "Baritone Sax",
        "Oboe", "English Horn",
        "Bassoon", "Clarinet",
        "Piccolo", "Flute",
        "Recorder", "Pan Flute",
        "Blown Bottle", "Shakuhachi",
        "Whistle", "Ocarina",
        "Lead 1 (square)", "Lead 2 (sawtooth)",
        "Lead 3 (calliope)", "Lead 4 (chiff)",
        "Lead 5 (charang)", "Lead 6 (voice)",
        "Lead 7 (fifths)", "Lead 8 (bass+lead)",
        "Pad 1 (new age)", "Pad 2 (warm)",
        "Pad 3 (polysynth)", "Pad 4 (choir)",
        "Pad 5 (bowed)", "Pad 6 (metallic)",
        "Pad 7 (halo)", "Pad 8 (sweep)",
        "FX 1 (rain)", "FX 2 (soundtrack)",
        "FX 3 (crystal)", "FX 4 (atmosphere)",
        "FX 5 (brightness)", "FX 6 (goblins)",
        "FX 7 (echoes)", "FX 8 (sci-fi)",
        "Sitar", "Banjo",
        "Shamisen", "Koto",
        "Kalimba", "Bagpipe",
        "Fiddle", "Shanai",
        "Tinkle Bell", "Agogo",
        "Steel Drums", "Woodblock",
        "Taiko Drum", "Melodic Tom",
        "Synth Drum", "Reverse Cymbal",
        "Guitar Fret Noise", "Breath Noise",
        "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter",
        "Applause", "Gunshot"
    };

    uint16_t ReadUint16BigEndian(const uint8_t* pData)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(pData[0]) << 8) | static_cast<uint16_t>(pData[1]));
    }

    uint32_t ReadUint32BigEndian(const uint8_t* pData)
    {
        return (static_cast<uint32_t>(pData[0]) << 24) | (static_cast<uint32_t>(pData[1]) << 16) | (static_cast<uint32_t>(pData[2]) << 8) | static_cast<uint32_t>(pData[3]);
    }

    uint32_t ReadUint24BigEndian(const uint8_t* pData)
    {
        return (static_cast<uint32_t>(pData[0]) << 16) | (static_cast<uint32_t>(pData[1]) << 8) | static_cast<uint32_t>(pData[2]);
    }

    uint32_t DecodeVariableLength(const uint8_t* pData, uint32_t& outValue)
    {
        outValue = 0;
        uint32_t bytesRead = 0;

        for (uint32_t i = 0; i < MidiConstants::MaxVlqBytes; ++i)
        {
            const uint8_t currentByte = pData[i];
            outValue = (outValue << MidiConstants::VlqShiftBits) | (currentByte & MidiConstants::VlqDataMask);
            ++bytesRead;

            if (0 == (currentByte & MidiConstants::VlqContinueBit))
            {
                break;
            }
        }

        return bytesRead;
    }

    struct PendingNote
    {
        uint32_t StartTick;
        uint8_t  Velocity;
    };

    using PendingNoteMap = std::map<uint16_t, PendingNote>;

    uint16_t MakeNoteKey(uint8_t channel, uint8_t noteNumber)
    {
        return static_cast<uint16_t>((channel << MidiConstants::ChannelShift) | noteNumber);
    }

    // Removes the note pending on `key` (if any) and returns it completed with
    // the given end tick. Channel and note number are recovered from the key,
    // which was built from them when the matching note-on was seen.
    std::optional<MidiNote> TakePendingNote(PendingNoteMap& pending, uint16_t key, uint32_t endTick)
    {
        const auto it = pending.find(key);
        if (pending.end() == it)
        {
            return std::nullopt;
        }

        MidiNote note;
        note.StartTick = it->second.StartTick;
        note.EndTick = endTick;
        note.Channel = static_cast<uint8_t>(key >> MidiConstants::ChannelShift);
        note.NoteNumber = static_cast<uint8_t>(key & MidiConstants::MaxNoteNumber);
        note.Velocity = it->second.Velocity;
        pending.erase(it);
        return note;
    }

    // Emits every still-sounding note, ending each at the given tick. Used when
    // a track ends without explicit note-offs for its open notes.
    void FlushPendingNotes(MidiTrack& track, const PendingNoteMap& pending, uint32_t endTick)
    {
        for (const auto& entry : pending)
        {
            MidiNote note;
            note.StartTick = entry.second.StartTick;
            note.EndTick = endTick;
            note.Channel = static_cast<uint8_t>(entry.first >> MidiConstants::ChannelShift);
            note.NoteNumber = static_cast<uint8_t>(entry.first & MidiConstants::MaxNoteNumber);
            note.Velocity = entry.second.Velocity;
            track.pNotes->push_back(note);
        }
    }

    void ParseTrackData(const uint8_t* pData, uint32_t dataLength, MidiTrack& track)
    {
        uint32_t offset = 0;
        uint32_t absoluteTick = 0;
        uint8_t runningStatus = 0;

        auto pPending = std::make_unique<PendingNoteMap>();

        while (offset < dataLength)
        {
            uint32_t deltaTime = 0;
            offset += DecodeVariableLength(pData + offset, deltaTime);
            absoluteTick += deltaTime;

            if (offset >= dataLength)
            {
                break;
            }

            uint8_t statusByte = pData[offset];

            if (0 != (statusByte & MidiStatus::DataBitMask))
            {
                runningStatus = statusByte;
                ++offset;
            }
            else
            {
                statusByte = runningStatus;
            }

            const uint8_t statusType = statusByte & MidiStatus::StatusMask;
            const uint8_t channel = statusByte & MidiStatus::ChannelMask;

            if (MidiStatus::NoteOn == statusType)
            {
                const uint8_t noteNumber = pData[offset++];
                const uint8_t velocity = pData[offset++];
                const uint16_t key = MakeNoteKey(channel, noteNumber);

                // A note-on first closes any note already sounding on this key;
                // a zero-velocity note-on is the conventional note-off encoding.
                std::optional<MidiNote> closed = TakePendingNote(*pPending, key, absoluteTick);
                if (true == closed.has_value())
                {
                    track.pNotes->push_back(*closed);
                }

                if (0 != velocity)
                {
                    PendingNote pending;
                    pending.StartTick = absoluteTick;
                    pending.Velocity = velocity;
                    (*pPending)[key] = pending;
                }
            }
            else if (MidiStatus::NoteOff == statusType)
            {
                const uint8_t noteNumber = pData[offset++];
                offset++;
                const uint16_t key = MakeNoteKey(channel, noteNumber);

                std::optional<MidiNote> closed = TakePendingNote(*pPending, key, absoluteTick);
                if (true == closed.has_value())
                {
                    track.pNotes->push_back(*closed);
                }
            }
            else if (MidiStatus::ProgramChange == statusType)
            {
                track.ProgramNumber = pData[offset++];
                track.PrimaryChannel = channel;
            }
            else if (MidiStatus::ChannelPressure == statusType)
            {
                offset++;
            }
            else if (MidiStatus::ControlChange == statusType || MidiStatus::PolyPressure == statusType || MidiStatus::PitchBend == statusType)
            {
                offset += 2;
            }
            else if (MidiStatus::MetaEvent == statusByte)
            {
                runningStatus = 0;
                const uint8_t metaType = pData[offset++];
                uint32_t metaLength = 0;
                offset += DecodeVariableLength(pData + offset, metaLength);

                if (MetaType::SetTempo == metaType && MidiConstants::TempoDataLength == metaLength)
                {
                    TempoEvent tempo;
                    tempo.Tick = absoluteTick;
                    tempo.MicrosecondsPerBeat = ReadUint24BigEndian(pData + offset);
                    track.pTempoChanges->push_back(tempo);
                }
                else if (MetaType::TrackName == metaType)
                {
                    track.Name = std::string(reinterpret_cast<const char*>(pData + offset), metaLength);
                }
                else if (MetaType::EndOfTrack == metaType)
                {
                    FlushPendingNotes(track, *pPending, absoluteTick);
                    break;
                }

                offset += metaLength;
            }
            else if (MidiStatus::SysExStart == statusByte || MidiStatus::SysExEnd == statusByte)
            {
                runningStatus = 0;
                uint32_t sysExLength = 0;
                offset += DecodeVariableLength(pData + offset, sysExLength);
                offset += sysExLength;
            }
        }
    }

    const char* GetInstrumentName(uint8_t programNumber)
    {
        if (GmProgramCount > programNumber)
        {
            return GeneralMidiPrograms[programNumber];
        }
        return "Unknown";
    }

    struct SmpteDivision
    {
        uint8_t TicksPerFrame;
        double FramesPerSecond;
    };

    SmpteDivision DecodeSmpteDivision(uint16_t division)
    {
        const int8_t rawFps = static_cast<int8_t>((division >> 8) & 0xFF);
        const uint8_t tpf = static_cast<uint8_t>(division & 0xFF);

        const int positiveFps = -static_cast<int>(rawFps);

        SmpteDivision out;
        out.TicksPerFrame = tpf;
        if (SmpteNtscFpsCode == positiveFps)
        {
            out.FramesPerSecond = MidiConstants::SmpteNtscFps;
        }
        else
        {
            out.FramesPerSecond = static_cast<double>(positiveFps);
        }

        return out;
    }

    // Builds the display summary for one track: note count, instrument name,
    // and pitch range. The pitch range uses std::minmax_element so there is no
    // explicit inner loop at the call site.
    TrackInfo BuildTrackSummary(const MidiTrack& track, uint32_t index)
    {
        TrackInfo info;
        info.TrackIndex = index;
        info.TrackName = track.Name;
        info.NoteCount = static_cast<uint32_t>(track.pNotes->size());
        info.PrimaryChannel = track.PrimaryChannel;
        info.ProgramNumber = track.ProgramNumber;

        if (MidiConstants::NoProgram != track.ProgramNumber)
        {
            info.InstrumentName = GetInstrumentName(track.ProgramNumber);
        }
        else
        {
            info.InstrumentName = "No Program";
        }

        if (0 == info.NoteCount)
        {
            info.LowestNote = 0;
            info.HighestNote = 0;
        }
        else
        {
            const auto range = std::minmax_element(track.pNotes->begin(), track.pNotes->end(),
                [](const MidiNote& a, const MidiNote& b)
                {
                    return a.NoteNumber < b.NoteNumber;
                });
            info.LowestNote = range.first->NoteNumber;
            info.HighestNote = range.second->NoteNumber;
        }

        return info;
    }
}

namespace MidiParser
{
    std::unique_ptr<MidiFile> Parse(const std::string& filePath)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (false == file.is_open())
        {
            throw std::runtime_error("Cannot open file: " + filePath);
        }

        const auto fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        auto pBuffer = std::make_unique<std::vector<uint8_t>>(fileSize);
        file.read(reinterpret_cast<char*>(pBuffer->data()), static_cast<std::streamsize>(fileSize));

        const uint8_t* pData = pBuffer->data();
        uint32_t offset = 0;

        const uint32_t headerMagic = ReadUint32BigEndian(pData + offset);
        if (MidiConstants::HeaderMagic != headerMagic)
        {
            throw std::runtime_error("Invalid MIDI file: MThd header not found");
        }
        offset += 4;

        const uint32_t headerLength = ReadUint32BigEndian(pData + offset);
        if (MidiConstants::HeaderDataLength != headerLength)
        {
            throw std::runtime_error("Invalid MIDI header length");
        }
        offset += 4;

        auto pMidiFile = std::make_unique<MidiFile>();
        pMidiFile->FormatType = ReadUint16BigEndian(pData + offset);
        offset += 2;

        pMidiFile->TrackCount = ReadUint16BigEndian(pData + offset);
        offset += 2;

        const uint16_t division = ReadUint16BigEndian(pData + offset);
        offset += 2;

        const bool isSmpte = (0 != (division & MidiConstants::SmpteBitMask));
        uint32_t smpteSyntheticMpb = 0;

        if (true == isSmpte)
        {
            const SmpteDivision smpte = DecodeSmpteDivision(division);
            if (0 == smpte.TicksPerFrame || 0.0 >= smpte.FramesPerSecond)
            {
                throw std::runtime_error("Invalid SMPTE division in MIDI header");
            }
            pMidiFile->TicksPerBeat = smpte.TicksPerFrame;
            smpteSyntheticMpb = static_cast<uint32_t>((static_cast<double>(MidiConstants::MicrosPerSecond) / smpte.FramesPerSecond) + RoundingBias);
        }
        else
        {
            pMidiFile->TicksPerBeat = division;
        }

        for (uint16_t i = 0; i < pMidiFile->TrackCount; ++i)
        {
            if (offset + TrackHeaderSize > fileSize)
            {
                break;
            }

            const uint32_t trackMagic = ReadUint32BigEndian(pData + offset);
            if (MidiConstants::TrackMagic != trackMagic)
            {
                throw std::runtime_error("Invalid track: MTrk header not found");
            }
            offset += 4;

            const uint32_t trackLength = ReadUint32BigEndian(pData + offset);
            offset += 4;

            MidiTrack track;
            ParseTrackData(pData + offset, trackLength, track);
            pMidiFile->pTracks->push_back(std::move(track));

            offset += trackLength;
        }

        if (true == isSmpte)
        {
            for (auto& track : *pMidiFile->pTracks)
            {
                track.pTempoChanges->clear();
            }
            if (false == pMidiFile->pTracks->empty())
            {
                TempoEvent synthetic;
                synthetic.Tick = 0;
                synthetic.MicrosecondsPerBeat = smpteSyntheticMpb;
                (*pMidiFile->pTracks)[0].pTempoChanges->push_back(synthetic);
            }
        }

        return pMidiFile;
    }

    std::unique_ptr<std::vector<TrackInfo>> GetTrackSummaries(const MidiFile& midiFile)
    {
        auto pSummaries = std::make_unique<std::vector<TrackInfo>>();

        for (uint32_t i = 0; i < midiFile.pTracks->size(); ++i)
        {
            pSummaries->push_back(BuildTrackSummary((*midiFile.pTracks)[i], i));
        }

        return pSummaries;
    }
}
