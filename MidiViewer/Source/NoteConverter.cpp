#include "NoteConverter.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace
{
    constexpr double A4Frequency = 440.0;
    constexpr int A4NoteNumber = 69;
    constexpr double SemitonesPerOctave = 12.0;
    constexpr double OctaveRatio = 2.0;
    constexpr double MaxMidiVelocity = 127.0;
    constexpr double MaxVelocity = 1.0;
    constexpr double MinDurationMs = 1.0;
    constexpr double UsToMsDivisor = 1000.0;
}

namespace NoteConverter
{
    std::unique_ptr<std::vector<TempoEvent>> BuildTempoMap(const MidiFile& midiFile)
    {
        auto pTempoMap = std::make_unique<std::vector<TempoEvent>>();

        for (const auto& track : *midiFile.pTracks)
        {
            for (const auto& tempo : *track.pTempoChanges)
            {
                pTempoMap->push_back(tempo);
            }
        }

        std::sort(pTempoMap->begin(), pTempoMap->end(), [](const TempoEvent& a, const TempoEvent& b)
            {
                return a.Tick < b.Tick;
            });

        if (true == pTempoMap->empty() || 0 != (*pTempoMap)[0].Tick)
        {
            TempoEvent defaultTempo;
            defaultTempo.Tick = 0;
            defaultTempo.MicrosecondsPerBeat = MidiConstants::DefaultUsPerBeat;
            pTempoMap->insert(pTempoMap->begin(), defaultTempo);
        }

        return pTempoMap;
    }

    double TickToMs(uint32_t tick, const std::vector<TempoEvent>& tempoMap, uint16_t ticksPerBeat)
    {
        double ms = 0.0;
        uint32_t prevTick = 0;
        uint32_t currentUsPerBeat = MidiConstants::DefaultUsPerBeat;

        if (false == tempoMap.empty())
        {
            currentUsPerBeat = tempoMap[0].MicrosecondsPerBeat;
        }

        for (size_t i = 1; i < tempoMap.size(); ++i)
        {
            if (tempoMap[i].Tick >= tick)
            {
                break;
            }

            const uint32_t deltaTicks = tempoMap[i].Tick - prevTick;
            ms += (static_cast<double>(deltaTicks) / static_cast<double>(ticksPerBeat)) * (static_cast<double>(currentUsPerBeat) / UsToMsDivisor);

            prevTick = tempoMap[i].Tick;
            currentUsPerBeat = tempoMap[i].MicrosecondsPerBeat;
        }

        const uint32_t remainingTicks = tick - prevTick;
        ms += (static_cast<double>(remainingTicks) / static_cast<double>(ticksPerBeat)) * (static_cast<double>(currentUsPerBeat) / UsToMsDivisor);

        return ms;
    }

    double MidiNoteToHz(uint8_t noteNumber)
    {
        return A4Frequency * std::pow(OctaveRatio, (static_cast<double>(noteNumber) - static_cast<double>(A4NoteNumber)) / SemitonesPerOctave);
    }

    double NormalizeVelocity(uint8_t velocity)
    {
        return static_cast<double>(velocity) / MaxMidiVelocity;
    }

    std::unique_ptr<std::vector<ConvertedNote>> ConvertNotes(const std::vector<MidiNote>& midiNotes, const std::vector<TempoEvent>& tempoMap, uint16_t ticksPerBeat, double velocityScale)
    {
        auto pConverted = std::make_unique<std::vector<ConvertedNote>>();
        pConverted->reserve(midiNotes.size());

        for (const auto& midi : midiNotes)
        {
            if (midi.EndTick <= midi.StartTick)
            {
                continue;
            }

            const double startMs = TickToMs(midi.StartTick, tempoMap, ticksPerBeat);
            const double endMs = TickToMs(midi.EndTick, tempoMap, ticksPerBeat);
            const double durationMs = endMs - startMs;

            if (MinDurationMs > durationMs)
            {
                continue;
            }

            double velocity = NormalizeVelocity(midi.Velocity) * velocityScale;
            if (MaxVelocity < velocity)
            {
                velocity = MaxVelocity;
            }

            ConvertedNote converted;
            converted.StartMs = static_cast<uint32_t>(std::round(startMs));
            converted.DurationMs = static_cast<uint32_t>(std::round(durationMs));
            converted.FrequencyHz = MidiNoteToHz(midi.NoteNumber);
            converted.Velocity = velocity;

            pConverted->push_back(converted);
        }

        std::sort(pConverted->begin(), pConverted->end(), [](const ConvertedNote& a, const ConvertedNote& b)
            {
                if (a.StartMs != b.StartMs)
                {
                    return a.StartMs < b.StartMs;
                }
                return a.FrequencyHz < b.FrequencyHz;
            });

        return pConverted;
    }

    std::unique_ptr<std::vector<ConvertedNote>> ApplySkylineFilter(const std::vector<ConvertedNote>& notes)
    {
        auto pFiltered = std::make_unique<std::vector<ConvertedNote>>();

        struct TimedEvent
        {
            uint32_t TimeMs;
            int Type;
            double FrequencyHz;
            double Velocity;
            size_t Index;
        };

        constexpr int EventNoteOn = 1;
        constexpr int EventNoteOff = 0;
        constexpr size_t EventsPerNote = 2;

        auto pEvents = std::make_unique<std::vector<TimedEvent>>();
        pEvents->reserve(notes.size() * EventsPerNote);

        for (size_t i = 0; i < notes.size(); ++i)
        {
            TimedEvent onEvent;
            onEvent.TimeMs = notes[i].StartMs;
            onEvent.Type = EventNoteOn;
            onEvent.FrequencyHz = notes[i].FrequencyHz;
            onEvent.Velocity = notes[i].Velocity;
            onEvent.Index = i;
            pEvents->push_back(onEvent);

            TimedEvent offEvent;
            offEvent.TimeMs = notes[i].StartMs + notes[i].DurationMs;
            offEvent.Type = EventNoteOff;
            offEvent.FrequencyHz = notes[i].FrequencyHz;
            offEvent.Velocity = notes[i].Velocity;
            offEvent.Index = i;
            pEvents->push_back(offEvent);
        }

        std::sort(pEvents->begin(), pEvents->end(), [](const TimedEvent& a, const TimedEvent& b)
            {
                if (a.TimeMs != b.TimeMs)
                {
                    return a.TimeMs < b.TimeMs;
                }
                return a.Type < b.Type;
            });

        auto pSounding = std::make_unique<std::map<size_t, std::pair<double, double>>>();

        uint32_t currentStart = 0;
        double currentFreq = 0.0;
        double currentVel = 0.0;
        size_t currentIdx = 0;
        bool hasActive = false;

        for (const auto& event : *pEvents)
        {
            if (EventNoteOn == event.Type)
            {
                (*pSounding)[event.Index] = { event.FrequencyHz, event.Velocity };
            }
            else
            {
                pSounding->erase(event.Index);
            }

            double bestFreq = 0.0;
            double bestVel = 0.0;
            size_t bestIdx = 0;

            for (const auto& pair : *pSounding)
            {
                if (pair.second.first > bestFreq)
                {
                    bestFreq = pair.second.first;
                    bestVel = pair.second.second;
                    bestIdx = pair.first;
                }
            }

            const bool topChanged = (bestFreq != currentFreq) || (bestIdx != currentIdx);

            if (true == hasActive && true == topChanged)
            {
                if (event.TimeMs > currentStart)
                {
                    ConvertedNote filtered;
                    filtered.StartMs = currentStart;
                    filtered.DurationMs = event.TimeMs - currentStart;
                    filtered.FrequencyHz = currentFreq;
                    filtered.Velocity = currentVel;
                    pFiltered->push_back(filtered);
                }
                hasActive = false;
            }

            if (0.0 < bestFreq && false == hasActive)
            {
                currentStart = event.TimeMs;
                currentFreq = bestFreq;
                currentVel = bestVel;
                currentIdx = bestIdx;
                hasActive = true;
            }
        }

        return pFiltered;
    }
}
