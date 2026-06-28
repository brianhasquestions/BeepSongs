#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/// @file MidiTypes.h
/// @brief Defines all data structures for representing parsed MIDI
///        file content including tracks, notes, and tempo events.

/// @brief MIDI status byte constants.
namespace MidiStatus
{
    constexpr uint8_t NoteOff         = 0x80;
    constexpr uint8_t NoteOn          = 0x90;
    constexpr uint8_t PolyPressure    = 0xA0;
    constexpr uint8_t ControlChange   = 0xB0;
    constexpr uint8_t ProgramChange   = 0xC0;
    constexpr uint8_t ChannelPressure = 0xD0;
    constexpr uint8_t PitchBend       = 0xE0;
    constexpr uint8_t MetaEvent       = 0xFF;
    constexpr uint8_t SysExStart      = 0xF0;
    constexpr uint8_t SysExEnd        = 0xF7;
    constexpr uint8_t ChannelMask     = 0x0F;
    constexpr uint8_t StatusMask      = 0xF0;
    constexpr uint8_t DataBitMask     = 0x80;
}

/// @brief MIDI meta event type constants.
namespace MetaType
{
    constexpr uint8_t TrackName     = 0x03;
    constexpr uint8_t EndOfTrack    = 0x2F;
    constexpr uint8_t SetTempo      = 0x51;
    constexpr uint8_t TimeSignature = 0x58;
}

/// @brief Constants for MIDI file parsing.
namespace MidiConstants
{
    constexpr uint32_t HeaderMagic       = 0x4D546864;
    constexpr uint32_t TrackMagic        = 0x4D54726B;
    constexpr uint32_t HeaderDataLength  = 6;
    constexpr uint32_t TempoDataLength   = 3;
    constexpr uint32_t MaxVlqBytes       = 4;
    constexpr uint8_t  VlqContinueBit    = 0x80;
    constexpr uint8_t  VlqDataMask       = 0x7F;
    constexpr uint8_t  VlqShiftBits      = 7;
    constexpr uint16_t SmpteBitMask      = 0x8000;
    constexpr uint8_t  ChannelShift      = 8;
    constexpr uint32_t DefaultUsPerBeat  = 500000;
    constexpr uint32_t MicrosPerSecond   = 1000000;
    constexpr uint8_t  MaxNoteNumber     = 127;
    constexpr uint8_t  NoProgram         = 0xFF;
    constexpr double   SmpteNtscFps      = 29.97;
}

/// @brief A tempo change event at a specific tick position.
struct TempoEvent
{
    /// @brief Absolute tick position of the tempo change.
    uint32_t Tick;

    /// @brief Tempo in microseconds per beat (from set_tempo meta event).
    uint32_t MicrosecondsPerBeat;
};

/// @brief A raw MIDI note extracted from note_on/note_off pairing.
struct MidiNote
{
    /// @brief Absolute tick position where the note starts.
    uint32_t StartTick;

    /// @brief Absolute tick position where the note ends.
    uint32_t EndTick;

    /// @brief MIDI channel (0-15).
    uint8_t Channel;

    /// @brief MIDI note number (0-127, where 60 = C4, 69 = A4).
    uint8_t NoteNumber;

    /// @brief MIDI velocity (0-127).
    uint8_t Velocity;
};

/// @brief Display-only summary information about a single track.
struct TrackInfo
{
    /// @brief Zero-based index of this track in the file.
    uint32_t TrackIndex;

    /// @brief Track name from meta event, or empty string.
    std::string TrackName;

    /// @brief Total number of notes in this track.
    uint32_t NoteCount;

    /// @brief Lowest MIDI note number in this track.
    uint8_t LowestNote;

    /// @brief Highest MIDI note number in this track.
    uint8_t HighestNote;

    /// @brief Most common channel used in this track.
    uint8_t PrimaryChannel;

    /// @brief General MIDI instrument name.
    std::string InstrumentName;

    /// @brief General MIDI program number (0-127).
    uint8_t ProgramNumber;
};

/// @brief A single parsed MIDI track containing notes and tempo events.
struct MidiTrack
{
    /// @brief Track name from meta event.
    std::string Name;

    /// @brief All notes extracted from this track.
    std::unique_ptr<std::vector<MidiNote>> pNotes;

    /// @brief Tempo change events found in this track.
    std::unique_ptr<std::vector<TempoEvent>> pTempoChanges;

    /// @brief Most common channel in this track.
    uint8_t PrimaryChannel;

    /// @brief General MIDI program number assigned to this track.
    uint8_t ProgramNumber;

    MidiTrack()
        : pNotes(std::make_unique<std::vector<MidiNote>>())
        , pTempoChanges(std::make_unique<std::vector<TempoEvent>>())
        , PrimaryChannel(0)
        , ProgramNumber(MidiConstants::NoProgram)
    {
    }
};

/// @brief Top-level structure representing an entire parsed MIDI file.
struct MidiFile
{
    /// @brief MIDI format type (0 = single track, 1 = multi-track).
    uint16_t FormatType;

    /// @brief Number of tracks declared in the header.
    uint16_t TrackCount;

    /// @brief Ticks per beat (quarter note) from the header division field.
    uint16_t TicksPerBeat;

    /// @brief All parsed tracks.
    std::unique_ptr<std::vector<MidiTrack>> pTracks;

    MidiFile()
        : FormatType(0)
        , TrackCount(0)
        , TicksPerBeat(0)
        , pTracks(std::make_unique<std::vector<MidiTrack>>())
    {
    }
};
