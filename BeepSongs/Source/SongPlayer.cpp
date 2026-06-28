#include <Windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#pragma comment(lib, "winmm.lib")

#include "SongPlayer.h"
#include "MusicConstants.h"

namespace
{
    constexpr DWORD SampleRate = 44100;
    constexpr WORD BitsPerSample = 16;
    constexpr WORD NumChannels = 1;
    constexpr WORD BitsPerByte = 8;
    constexpr DWORD MonoReserveSeconds = 120;
    constexpr double TwoPi = 6.28318530717958647692;
    constexpr double MaxAmplitude = 30000.0;
    constexpr double PeakLimit = 32000.0;
    constexpr double StreamPerNoteAmplitude = 10000.0;
    constexpr DWORD FadeSamples = 200;
    constexpr double DecayRate = 2.0;
    constexpr DWORD MsPerSecond = 1000;
    constexpr DWORD StreamChunkMs = 100;
    constexpr DWORD StreamChunkSamples = (SampleRate * StreamChunkMs) / MsPerSecond;
    constexpr DWORD StreamNumChunks = 4;
    constexpr DWORD StreamPollMs = 50;
    constexpr DWORD StreamTailPaddingMs = 500;

    // Flag set by the Ctrl+C console handler to signal
    // that playback should be stopped immediately.
    std::atomic<bool> StopRequested(false);

    // Tracks whether the player is active (synthesizing or playing).
    // When false, Ctrl+C falls through to the default handler
    // which terminates the program normally.
    std::atomic<bool> IsPlaying(false);

    BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
    {
        if (CTRL_C_EVENT == ctrlType && true == IsPlaying.load())
        {
            StopRequested.store(true);
            return TRUE;
        }
        return FALSE;
    }

    void SynthesizeNote(std::vector<short>& buffer, const Note& note)
    {
        const DWORD numSamples = (SampleRate * note.DurationMs) / MsPerSecond;

        if (Music::Frequency::Rest == note.Frequency)
        {
            buffer.insert(buffer.end(), numSamples, 0);
            return;
        }

        const double frequency = static_cast<double>(note.Frequency);
        const double phaseStep = (TwoPi * frequency) / static_cast<double>(SampleRate);

        const DWORD fadeLength = (FadeSamples * 2 > numSamples) ? numSamples / 2 : FadeSamples;

        for (DWORD i = 0; i < numSamples; ++i)
        {
            double amplitude = MaxAmplitude;

            if (i < fadeLength)
            {
                amplitude *= static_cast<double>(i) / static_cast<double>(fadeLength);
            }
            else if (i >= numSamples - fadeLength)
            {
                amplitude *= static_cast<double>(numSamples - i - 1) / static_cast<double>(fadeLength);
            }

            const double sample = amplitude * std::sin(phaseStep * static_cast<double>(i));

            buffer.push_back(static_cast<short>(sample));
        }
    }

    void PlayBuffer(const std::vector<short>& buffer)
    {
        if (buffer.empty())
        {
            return;
        }

        WAVEFORMATEX waveFormat = {};
        waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        waveFormat.nChannels = NumChannels;
        waveFormat.nSamplesPerSec = SampleRate;
        waveFormat.wBitsPerSample = BitsPerSample;
        waveFormat.nBlockAlign = static_cast<WORD>(NumChannels * BitsPerSample / BitsPerByte);
        waveFormat.nAvgBytesPerSec = SampleRate * waveFormat.nBlockAlign;
        waveFormat.cbSize = 0;

        HWAVEOUT hWaveOut = nullptr;
        MMRESULT result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL);

        if (MMSYSERR_NOERROR != result)
        {
            return;
        }

        StopRequested.store(false);

        WAVEHDR waveHeader = {};
        waveHeader.lpData = reinterpret_cast<LPSTR>(const_cast<short*>(buffer.data()));
        waveHeader.dwBufferLength = static_cast<DWORD>(buffer.size() * sizeof(short));

        waveOutPrepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &waveHeader, sizeof(WAVEHDR));

        // Poll for completion or Ctrl+C
        while (0 == (waveHeader.dwFlags & WHDR_DONE))
        {
            if (true == StopRequested.load())
            {
                waveOutReset(hWaveOut);
                break;
            }
            Sleep(StreamPollMs);
        }

        waveOutUnprepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
        StopRequested.store(false);
    }
}

namespace SongPlayer
{
    PHANDLER_ROUTINE GetCtrlHandler()
    {
        return ConsoleCtrlHandler;
    }

    void PlaySong(const std::vector<Note>& notes)
    {
        IsPlaying.store(true);

        std::vector<short> buffer;
        buffer.reserve(SampleRate * MonoReserveSeconds);

        for (const auto& currentNote : notes)
        {
            SynthesizeNote(buffer, currentNote);
        }

        PlayBuffer(buffer);
        IsPlaying.store(false);
    }

    struct StreamState
    {
        const TimedNote* pNotes;
        size_t NoteCount;
        std::vector<size_t>* pActiveIndices;
        std::vector<double>* pMix;
        size_t NextUnscheduled;
        DWORD ChunkStartSample;
        DWORD SongEndSample;
    };

    struct MixContext
    {
        const TimedNote* pNote;
        std::vector<double>* pMix;
        DWORD ChunkStartSample;
    };

    DWORD MsToSamples(DWORD ms)
    {
        return static_cast<DWORD>((static_cast<uint64_t>(SampleRate) * ms) / MsPerSecond);
    }

    void MixOneNote(const MixContext& ctx)
    {
        const TimedNote& note = *ctx.pNote;

        const DWORD noteStartSample = MsToSamples(note.StartMs);
        const DWORD noteDurSamples = MsToSamples(note.DurationMs);
        if (0 == noteDurSamples)
        {
            return;
        }
        const DWORD noteEndSample = noteStartSample + noteDurSamples;

        const DWORD chunkStart = ctx.ChunkStartSample;
        const DWORD chunkEnd = chunkStart + StreamChunkSamples;

        const DWORD overlapBegin = (noteStartSample > chunkStart) ? noteStartSample : chunkStart;
        const DWORD overlapEnd = (noteEndSample < chunkEnd) ? noteEndSample : chunkEnd;
        if (overlapEnd <= overlapBegin)
        {
            return;
        }

        const double phaseStep = (TwoPi * note.FrequencyHz) / static_cast<double>(SampleRate);
        const double noteAmp = StreamPerNoteAmplitude * note.Velocity;
        const DWORD fadeLength = (FadeSamples * 2 > noteDurSamples) ? (noteDurSamples / 2) : FadeSamples;

        auto& mix = *ctx.pMix;

        for (DWORD global = overlapBegin; global < overlapEnd; ++global)
        {
            const DWORD noteIdx = global - noteStartSample;
            const DWORD chunkIdx = global - chunkStart;

            const double progress = static_cast<double>(noteIdx) / static_cast<double>(noteDurSamples);
            const double decay = std::exp(-DecayRate * progress);

            double amplitude = noteAmp * decay;
            if (noteIdx < fadeLength)
            {
                amplitude *= static_cast<double>(noteIdx) / static_cast<double>(fadeLength);
            }

            mix[chunkIdx] += amplitude * std::sin(phaseStep * static_cast<double>(noteIdx));
        }
    }

    void FillChunk(short* pDest, StreamState* pState)
    {
        auto& mix = *pState->pMix;
        std::fill(mix.begin(), mix.end(), 0.0);

        const DWORD chunkStart = pState->ChunkStartSample;
        const DWORD chunkEnd = chunkStart + StreamChunkSamples;

        auto& active = *pState->pActiveIndices;
        const TimedNote* notes = pState->pNotes;
        const size_t noteCount = pState->NoteCount;

        active.erase(std::remove_if(active.begin(), active.end(),
            [chunkStart, notes](size_t idx)
            {
                const TimedNote& n = notes[idx];
                const DWORD endSample = MsToSamples(n.StartMs + n.DurationMs);
                return (endSample <= chunkStart);
            }), active.end());

        while (pState->NextUnscheduled < noteCount)
        {
            const TimedNote& n = notes[pState->NextUnscheduled];
            const DWORD startSample = MsToSamples(n.StartMs);
            if (startSample >= chunkEnd)
            {
                break;
            }
            active.push_back(pState->NextUnscheduled);
            ++pState->NextUnscheduled;
        }

        for (const size_t idx : active)
        {
            MixContext ctx;
            ctx.pNote = &notes[idx];
            ctx.pMix = pState->pMix;
            ctx.ChunkStartSample = chunkStart;
            MixOneNote(ctx);
        }

        for (DWORD i = 0; i < StreamChunkSamples; ++i)
        {
            double s = mix[i];
            if (PeakLimit < s)
            {
                s = PeakLimit;
            }
            if (-PeakLimit > s)
            {
                s = -PeakLimit;
            }
            pDest[i] = static_cast<short>(s);
        }
    }

    DWORD ComputeSongEndSample(const TimedNote* notes, size_t count)
    {
        DWORD maxEndMs = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const DWORD endMs = notes[i].StartMs + notes[i].DurationMs;
            if (endMs > maxEndMs)
            {
                maxEndMs = endMs;
            }
        }

        return MsToSamples(maxEndMs + StreamTailPaddingMs);
    }

    void PlayTimedSongStreaming(const TimedNote* notes, size_t count)
    {
        const DWORD songEndSample = ComputeSongEndSample(notes, count);

        const HANDLE hDone = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (nullptr == hDone)
        {
            return;
        }

        WAVEFORMATEX fmt = {};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = NumChannels;
        fmt.nSamplesPerSec = SampleRate;
        fmt.wBitsPerSample = BitsPerSample;
        fmt.nBlockAlign = static_cast<WORD>((NumChannels * BitsPerSample) / BitsPerByte);
        fmt.nAvgBytesPerSec = SampleRate * fmt.nBlockAlign;

        HWAVEOUT hWaveOut = nullptr;
        if (MMSYSERR_NOERROR != waveOutOpen(&hWaveOut, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(hDone), 0, CALLBACK_EVENT))
        {
            CloseHandle(hDone);
            return;
        }

        auto pBuffers = std::make_unique<std::array<std::vector<short>, StreamNumChunks>>();
        auto pHdrs = std::make_unique<std::array<WAVEHDR, StreamNumChunks>>();
        auto pMix = std::make_unique<std::vector<double>>(StreamChunkSamples, 0.0);
        auto pActive = std::make_unique<std::vector<size_t>>();

        for (DWORD i = 0; i < StreamNumChunks; ++i)
        {
            (*pBuffers)[i].assign(StreamChunkSamples, 0);
            WAVEHDR& h = (*pHdrs)[i];
            ZeroMemory(&h, sizeof(WAVEHDR));
            h.lpData = reinterpret_cast<LPSTR>((*pBuffers)[i].data());
            h.dwBufferLength = static_cast<DWORD>(StreamChunkSamples * sizeof(short));
            waveOutPrepareHeader(hWaveOut, &h, sizeof(WAVEHDR));
        }

        StreamState state;
        state.pNotes = notes;
        state.NoteCount = count;
        state.pActiveIndices = pActive.get();
        state.pMix = pMix.get();
        state.NextUnscheduled = 0;
        state.ChunkStartSample = 0;
        state.SongEndSample = songEndSample;

        StopRequested.store(false);

        for (DWORD i = 0; i < StreamNumChunks; ++i)
        {
            FillChunk((*pBuffers)[i].data(), &state);
            state.ChunkStartSample += StreamChunkSamples;
            waveOutWrite(hWaveOut, &(*pHdrs)[i], sizeof(WAVEHDR));
        }

        bool moreAudioToQueue = (state.ChunkStartSample < state.SongEndSample);
        std::array<bool, StreamNumChunks> finalized{};
        DWORD chunksRemaining = StreamNumChunks;

        while (chunksRemaining > 0)
        {
            WaitForSingleObject(hDone, StreamPollMs);

            if (true == StopRequested.load())
            {
                waveOutReset(hWaveOut);
                break;
            }

            for (DWORD i = 0; i < StreamNumChunks; ++i)
            {
                if (true == finalized[i])
                {
                    continue;
                }
                WAVEHDR& h = (*pHdrs)[i];
                if (0 == (h.dwFlags & WHDR_DONE))
                {
                    continue;
                }

                if (true == moreAudioToQueue)
                {
                    FillChunk((*pBuffers)[i].data(), &state);
                    state.ChunkStartSample += StreamChunkSamples;
                    waveOutWrite(hWaveOut, &h, sizeof(WAVEHDR));
                    if (state.ChunkStartSample >= state.SongEndSample)
                    {
                        moreAudioToQueue = false;
                    }
                }
                else
                {
                    finalized[i] = true;
                    --chunksRemaining;
                }
            }
        }

        for (DWORD i = 0; i < StreamNumChunks; ++i)
        {
            waveOutUnprepareHeader(hWaveOut, &(*pHdrs)[i], sizeof(WAVEHDR));
        }
        waveOutClose(hWaveOut);
        CloseHandle(hDone);

        StopRequested.store(false);
    }

    void PlayTimedSong(const std::vector<TimedNote>& timedNotes)
    {
        PlayTimedSong(timedNotes.data(), timedNotes.size());
    }

    void PlayTimedSong(const TimedNote* notes, size_t count)
    {
        if (nullptr == notes || 0 == count)
        {
            return;
        }

        IsPlaying.store(true);
        PlayTimedSongStreaming(notes, count);
        IsPlaying.store(false);
    }
}
