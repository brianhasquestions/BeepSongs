#include "AudioEngine.h"
#include <cmath>
#include <cstring>

namespace
{
    constexpr double TwoPi = Audio::Pi * 2.0;
}

AudioEngine::AudioEngine()
    : WaveOut(nullptr), pActivePattern(nullptr), Running(false), WaveHeaders{},
      SamplePositionInStep(0), PlaybackStep(0), SoundQueue{}, QueueWritePos(0),
      QueueReadPos(0), OneShotPlaying(false), OneShotOffset(0), OneShotPhase(0.0), ActiveOneShot{}
{
}

AudioEngine::~AudioEngine()
{
    if (Running.load()) { Stop(); }
}

bool AudioEngine::Start(Pattern* pPattern)
{
    pActivePattern = pPattern;

    // Pre-allocate to MaxRows so dynamic row additions never cause reallocation
    pPhases = std::make_unique<std::vector<double>>(static_cast<size_t>(Sequencer::MaxRows), 0.0);
    pDrumOffsets = std::make_unique<std::vector<DWORD>>(static_cast<size_t>(Sequencer::MaxRows), 0);
    pDrumActive = std::make_unique<std::vector<bool>>(static_cast<size_t>(Sequencer::MaxRows), false);

    SamplePositionInStep = 0;
    PlaybackStep = 0;
    pActivePattern->CurrentStep.store(0);

    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = Audio::NumChannels;
    waveFormat.nSamplesPerSec = Audio::SampleRate;
    waveFormat.wBitsPerSample = Audio::BitsPerSample;
    waveFormat.nBlockAlign = static_cast<WORD>(Audio::NumChannels * Audio::BitsPerSample / 8);
    waveFormat.nAvgBytesPerSec = Audio::SampleRate * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    MMRESULT result = waveOutOpen(&WaveOut, WAVE_MAPPER, &waveFormat, reinterpret_cast<DWORD_PTR>(WaveOutCallback), reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION);
    if (MMSYSERR_NOERROR != result) { return false; }

    Running.store(true);

    for (DWORD i = 0; i < AudioEngineConstants::BufferCount; ++i)
    {
        Buffers[i] = std::make_unique<short[]>(AudioEngineConstants::SamplesPerBuffer);
        std::memset(&WaveHeaders[i], 0, sizeof(WAVEHDR));
        WaveHeaders[i].lpData = reinterpret_cast<LPSTR>(Buffers[i].get());
        WaveHeaders[i].dwBufferLength = static_cast<DWORD>(AudioEngineConstants::SamplesPerBuffer * sizeof(short));
        waveOutPrepareHeader(WaveOut, &WaveHeaders[i], sizeof(WAVEHDR));
        FillBuffer(&WaveHeaders[i]);
        waveOutWrite(WaveOut, &WaveHeaders[i], sizeof(WAVEHDR));
    }

    return true;
}

void AudioEngine::Stop()
{
    Running.store(false);
    if (nullptr != WaveOut)
    {
        waveOutReset(WaveOut);
        for (DWORD i = 0; i < AudioEngineConstants::BufferCount; ++i)
        {
            waveOutUnprepareHeader(WaveOut, &WaveHeaders[i], sizeof(WAVEHDR));
        }
        waveOutClose(WaveOut);
        WaveOut = nullptr;
    }
}

bool AudioEngine::IsRunning() const { return Running.load(); }

void AudioEngine::QueueSound(const OneShotSound& sound)
{
    const int writePos = QueueWritePos.load();
    const int nextPos = (writePos + 1) % AudioEngineConstants::QueueCapacity;
    SoundQueue[static_cast<size_t>(writePos)] = sound;
    QueueWritePos.store(nextPos);
}

void CALLBACK AudioEngine::WaveOutCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR /*dwParam2*/)
{
    (void)hwo;
    if (WOM_DONE == uMsg)
    {
        auto* pEngine = reinterpret_cast<AudioEngine*>(dwInstance);
        auto* pHeader = reinterpret_cast<WAVEHDR*>(dwParam1);
        if (pEngine->Running.load())
        {
            pEngine->FillBuffer(pHeader);
            waveOutWrite(pEngine->WaveOut, pHeader, sizeof(WAVEHDR));
        }
    }
}

void AudioEngine::FillBuffer(WAVEHDR* pHeader)
{
    auto* pSamples = reinterpret_cast<short*>(pHeader->lpData);
    const DWORD sampleCount = AudioEngineConstants::SamplesPerBuffer;
    const DWORD stepDuration = pActivePattern->GetStepDurationSamples();
    const int rowCount = pActivePattern->GetRowCount();

    for (DWORD s = 0; s < sampleCount; ++s)
    {
        if (SamplePositionInStep >= stepDuration)
        {
            SamplePositionInStep = 0;
            PlaybackStep = (PlaybackStep + 1) % Sequencer::StepCount;
            pActivePattern->CurrentStep.store(PlaybackStep);

            for (int r = 0; r < rowCount && r < Sequencer::MaxRows; ++r)
            {
                const auto& row = pActivePattern->GetRow(r);
                if (InstrumentType::Drum == row.Type && pActivePattern->GetStep(r, PlaybackStep))
                {
                    (*pDrumOffsets)[static_cast<size_t>(r)] = 0;
                    (*pDrumActive)[static_cast<size_t>(r)] = true;
                }
            }
        }

        double mixSample = 0.0;

        for (int r = 0; r < rowCount && r < Sequencer::MaxRows; ++r)
        {
            const auto& row = pActivePattern->GetRow(r);
            const size_t ri = static_cast<size_t>(r);

            if (InstrumentType::Synth == row.Type && pActivePattern->GetStep(r, PlaybackStep))
            {
                double sample = (Mod::None != row.Modulation)
                    ? Synth.GenerateModulatedSample(row.Waveform, row.FrequencyHz, row.Modulation, (*pPhases)[ri], SamplePositionInStep)
                    : Synth.GenerateWaveformSample(row.Waveform, (*pPhases)[ri]);

                if (0 != (row.Modulation & Mod::Adsr)) { sample = Synth.ApplyAdsr(sample, SamplePositionInStep, stepDuration); }

                mixSample += sample * row.Volume * Audio::MaxAmplitude;
                const double phaseStep = (TwoPi * row.FrequencyHz) / static_cast<double>(Audio::SampleRate);
                (*pPhases)[ri] += phaseStep;
                if ((*pPhases)[ri] >= TwoPi) { (*pPhases)[ri] -= TwoPi; }
                continue;
            }

            if (InstrumentType::Drum != row.Type || !(*pDrumActive)[ri]) { continue; }

            const DWORD maxDur = Synthesizer::GetDrumDurationSamples(row.Drum);
            if ((*pDrumOffsets)[ri] >= maxDur) { (*pDrumActive)[ri] = false; continue; }

            const double sample = Synth.GenerateDrumSample(row.Drum, (*pDrumOffsets)[ri]);
            mixSample += sample * row.Volume * Audio::MaxAmplitude;
            (*pDrumOffsets)[ri]++;
        }

        // One-shot queue: when current finishes or nothing playing, pop next from queue
        if (OneShotPlaying && OneShotOffset >= ActiveOneShot.DurationSamples) { OneShotPlaying = false; }

        if (!OneShotPlaying && QueueReadPos != QueueWritePos.load())
        {
            ActiveOneShot = SoundQueue[static_cast<size_t>(QueueReadPos)];
            QueueReadPos = (QueueReadPos + 1) % AudioEngineConstants::QueueCapacity;
            OneShotOffset = 0;
            OneShotPhase = 0.0;
            OneShotPlaying = true;
        }

        if (OneShotPlaying && InstrumentType::Synth == ActiveOneShot.Type)
        {
            double sample = (Mod::None != ActiveOneShot.Modulation)
                ? Synth.GenerateModulatedSample(ActiveOneShot.Waveform, ActiveOneShot.FrequencyHz, ActiveOneShot.Modulation, OneShotPhase, OneShotOffset)
                : Synth.GenerateWaveformSample(ActiveOneShot.Waveform, OneShotPhase);

            if (0 != (ActiveOneShot.Modulation & Mod::Adsr)) { sample = Synth.ApplyAdsr(sample, OneShotOffset, ActiveOneShot.DurationSamples); }

            mixSample += sample * ActiveOneShot.Volume * Audio::MaxAmplitude;
            const double phaseStep = (TwoPi * ActiveOneShot.FrequencyHz) / static_cast<double>(Audio::SampleRate);
            OneShotPhase += phaseStep;
            if (OneShotPhase >= TwoPi) { OneShotPhase -= TwoPi; }
            ++OneShotOffset;
        }
        else if (OneShotPlaying)
        {
            const double sample = Synth.GenerateDrumSample(ActiveOneShot.Drum, OneShotOffset);
            mixSample += sample * ActiveOneShot.Volume * Audio::MaxAmplitude;
            ++OneShotOffset;
        }

        if (mixSample > Audio::PeakClamp) { mixSample = Audio::PeakClamp; }
        if (mixSample < -Audio::PeakClamp) { mixSample = -Audio::PeakClamp; }
        pSamples[s] = static_cast<short>(mixSample);
        ++SamplePositionInStep;
    }
}
