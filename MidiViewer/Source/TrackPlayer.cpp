#include "TrackPlayer.h"

#include <Windows.h>
#include <mmsystem.h>
#include <cmath>
#include <memory>
#include <vector>

#include "NoteConverter.h"

#pragma comment(lib, "winmm.lib")

namespace
{
    constexpr DWORD SampleRate = 44100;
    constexpr WORD BitsPerSample = 16;
    constexpr WORD NumChannels = 1;
    constexpr WORD BitsPerByte = 8;
    constexpr double TwoPi = 6.28318530717958647692;
    constexpr double MaxAmplitude = 30000.0;
    constexpr double PeakLimit = 32000.0;
    constexpr double DecayRate = 2.0;
    constexpr DWORD FadeSamples = 200;
    constexpr DWORD TailPaddingMs = 400;
    constexpr DWORD MsPerSecond = 1000;
    constexpr DWORD PollIntervalMs = 30;

    HANDLE g_hThread = nullptr;
    HANDLE g_hStopEvent = nullptr;
    HWAVEOUT g_hWaveOut = nullptr;
    ULONGLONG g_startTick = 0;
    std::unique_ptr<std::vector<short>> g_pPcmBuffer;
    std::unique_ptr<WAVEHDR> g_pWaveHdr;
    std::unique_ptr<std::vector<ConvertedNote>> g_pSourceNotes;

    std::unique_ptr<std::vector<short>> SynthesizeMix(const std::vector<ConvertedNote>& notes)
    {
        DWORD maxEndMs = 0;
        for (const auto& note : notes)
        {
            const DWORD endMs = note.StartMs + note.DurationMs;
            if (endMs > maxEndMs)
            {
                maxEndMs = endMs;
            }
        }

        const unsigned long long totalSamples64 = (static_cast<unsigned long long>(SampleRate) * (maxEndMs + TailPaddingMs)) / MsPerSecond;
        const DWORD totalSamples = static_cast<DWORD>(totalSamples64);

        auto pMix = std::make_unique<std::vector<double>>(totalSamples, 0.0);

        for (const auto& note : notes)
        {
            const DWORD startSample = static_cast<DWORD>((static_cast<unsigned long long>(SampleRate) * note.StartMs) / MsPerSecond);
            const DWORD numSamples = static_cast<DWORD>((static_cast<unsigned long long>(SampleRate) * note.DurationMs) / MsPerSecond);

            if (0 == numSamples || 0.0 >= note.FrequencyHz)
            {
                continue;
            }

            const double phaseStep = (TwoPi * note.FrequencyHz) / static_cast<double>(SampleRate);
            const double noteAmp = MaxAmplitude * note.Velocity;
            const DWORD fadeLength = (FadeSamples * 2 > numSamples) ? numSamples / 2 : FadeSamples;

            for (DWORD i = 0; i < numSamples; ++i)
            {
                const DWORD idx = startSample + i;
                if (idx >= totalSamples)
                {
                    break;
                }

                const double progress = static_cast<double>(i) / static_cast<double>(numSamples);
                const double decay = std::exp(-DecayRate * progress);
                double amp = noteAmp * decay;

                if (i < fadeLength)
                {
                    amp *= static_cast<double>(i) / static_cast<double>(fadeLength);
                }

                (*pMix)[idx] += amp * std::sin(phaseStep * static_cast<double>(i));
            }
        }

        double peak = 0.0;
        for (const double sample : *pMix)
        {
            const double abs = (0.0 > sample) ? -sample : sample;
            if (abs > peak)
            {
                peak = abs;
            }
        }
        const double scale = (PeakLimit < peak) ? (PeakLimit / peak) : 1.0;

        auto pOut = std::make_unique<std::vector<short>>(totalSamples);
        for (DWORD i = 0; i < totalSamples; ++i)
        {
            (*pOut)[i] = static_cast<short>((*pMix)[i] * scale);
        }

        return pOut;
    }

    void CloseWaveOut()
    {
        if (nullptr != g_hWaveOut)
        {
            if (nullptr != g_pWaveHdr)
            {
                waveOutUnprepareHeader(g_hWaveOut, g_pWaveHdr.get(), sizeof(WAVEHDR));
            }
            waveOutClose(g_hWaveOut);
            g_hWaveOut = nullptr;
        }
        g_pWaveHdr.reset();
        g_pPcmBuffer.reset();
    }

    DWORD WINAPI PlaybackThreadProc(LPVOID)
    {
        while (true)
        {
            if (nullptr != g_pWaveHdr && 0 != (g_pWaveHdr->dwFlags & WHDR_DONE))
            {
                break;
            }
            const DWORD wait = WaitForSingleObject(g_hStopEvent, PollIntervalMs);
            if (WAIT_OBJECT_0 == wait)
            {
                if (nullptr != g_hWaveOut)
                {
                    waveOutReset(g_hWaveOut);
                }
                break;
            }
        }

        return 0;
    }

    void JoinIfFinished()
    {
        if (nullptr == g_hThread)
        {
            return;
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(g_hThread, 0))
        {
            CloseHandle(g_hThread);
            g_hThread = nullptr;
            if (nullptr != g_hStopEvent)
            {
                CloseHandle(g_hStopEvent);
                g_hStopEvent = nullptr;
            }
            CloseWaveOut();
            g_pSourceNotes.reset();
        }
    }
}

namespace TrackPlayer
{
    bool Start(std::unique_ptr<std::vector<ConvertedNote>> pNotes)
    {
        JoinIfFinished();

        if (nullptr != g_hThread)
        {
            return false;
        }
        if (nullptr == pNotes || true == pNotes->empty())
        {
            return false;
        }

        g_pSourceNotes = std::move(pNotes);
        g_pPcmBuffer = SynthesizeMix(*g_pSourceNotes);

        if (nullptr == g_pPcmBuffer || true == g_pPcmBuffer->empty())
        {
            g_pSourceNotes.reset();
            return false;
        }

        WAVEFORMATEX fmt = { 0 };
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = NumChannels;
        fmt.nSamplesPerSec = SampleRate;
        fmt.wBitsPerSample = BitsPerSample;
        fmt.nBlockAlign = static_cast<WORD>((NumChannels * BitsPerSample) / BitsPerByte);
        fmt.nAvgBytesPerSec = SampleRate * fmt.nBlockAlign;
        fmt.cbSize = 0;

        if (MMSYSERR_NOERROR != waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL))
        {
            g_hWaveOut = nullptr;
            g_pPcmBuffer.reset();
            g_pSourceNotes.reset();
            return false;
        }

        g_pWaveHdr = std::make_unique<WAVEHDR>();
        ZeroMemory(g_pWaveHdr.get(), sizeof(WAVEHDR));
        g_pWaveHdr->lpData = reinterpret_cast<LPSTR>(g_pPcmBuffer->data());
        g_pWaveHdr->dwBufferLength = static_cast<DWORD>(g_pPcmBuffer->size() * sizeof(short));

        if (MMSYSERR_NOERROR != waveOutPrepareHeader(g_hWaveOut, g_pWaveHdr.get(), sizeof(WAVEHDR)))
        {
            CloseWaveOut();
            g_pSourceNotes.reset();
            return false;
        }
        if (MMSYSERR_NOERROR != waveOutWrite(g_hWaveOut, g_pWaveHdr.get(), sizeof(WAVEHDR)))
        {
            CloseWaveOut();
            g_pSourceNotes.reset();
            return false;
        }

        g_startTick = GetTickCount64();

        g_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (nullptr == g_hStopEvent)
        {
            waveOutReset(g_hWaveOut);
            CloseWaveOut();
            g_pSourceNotes.reset();
            return false;
        }

        g_hThread = CreateThread(nullptr, 0, PlaybackThreadProc, nullptr, 0, nullptr);
        if (nullptr == g_hThread)
        {
            waveOutReset(g_hWaveOut);
            CloseWaveOut();
            CloseHandle(g_hStopEvent);
            g_hStopEvent = nullptr;
            g_pSourceNotes.reset();
            return false;
        }

        return true;
    }

    void Stop()
    {
        if (nullptr == g_hThread)
        {
            return;
        }

        if (nullptr != g_hStopEvent)
        {
            SetEvent(g_hStopEvent);
        }

        WaitForSingleObject(g_hThread, INFINITE);
        CloseHandle(g_hThread);
        g_hThread = nullptr;

        if (nullptr != g_hStopEvent)
        {
            CloseHandle(g_hStopEvent);
            g_hStopEvent = nullptr;
        }

        CloseWaveOut();
        g_pSourceNotes.reset();
        g_startTick = 0;
    }

    bool IsPlaying()
    {
        JoinIfFinished();

        return (nullptr != g_hThread);
    }

    unsigned int GetElapsedMs()
    {
        if (nullptr == g_hThread || 0 == g_startTick)
        {
            return 0;
        }

        return static_cast<unsigned int>(GetTickCount64() - g_startTick);
    }
}
