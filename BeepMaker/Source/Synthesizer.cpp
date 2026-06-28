#include "Synthesizer.h"
#include <cmath>
#include <cstdint>

namespace
{
    constexpr double TwoPi = Audio::Pi * 2.0;
    constexpr double SemitonesPerOctave = 12.0;

    constexpr double KickBaseFreq = 40.0;
    constexpr double KickSweepRange = 110.0;
    constexpr double KickSweepRate = 40.0;
    constexpr double KickDecayRate = 8.0;
    constexpr DWORD KickDurationMs = 200;

    constexpr double SnareBodyFreq = 200.0;
    constexpr double SnareBodyMix = 0.6;
    constexpr double SnareNoiseMix = 0.8;
    constexpr double SnareBodyDecay = 25.0;
    constexpr double SnareNoiseDecay = 15.0;
    constexpr DWORD SnareDurationMs = 150;

    constexpr double ClosedHatDecay = 60.0;
    constexpr DWORD ClosedHatDurationMs = 50;

    constexpr double OpenHatDecay = 10.0;
    constexpr DWORD OpenHatDurationMs = 300;

    constexpr double ClapBurstDecay = 50.0;
    constexpr double ClapTailDecay = 15.0;
    constexpr double ClapTailMix = 0.5;
    constexpr double ClapBurst1Ms = 0.0;
    constexpr double ClapBurst2Ms = 10.0;
    constexpr double ClapBurst3Ms = 20.0;
    constexpr double ClapTailStartMs = 30.0;
    constexpr DWORD ClapDurationMs = 200;

    // 808 Kick: deep sub-bass boom
    constexpr double Kick808BaseFreq = 30.0;
    constexpr double Kick808SweepRange = 120.0;
    constexpr double Kick808SweepRate = 20.0;
    constexpr double Kick808DecayRate = 4.0;
    constexpr DWORD Kick808DurationMs = 500;

    // 808 Snare: pitched body + noise
    constexpr double Snare808BodyFreq = 180.0;
    constexpr double Snare808BodyMix = 0.5;
    constexpr double Snare808NoiseMix = 0.9;
    constexpr double Snare808BodyDecay = 20.0;
    constexpr double Snare808NoiseDecay = 12.0;
    constexpr DWORD Snare808DurationMs = 200;

    // 808 Closed Hat: metallic
    constexpr double Hat808Freq1 = 3500.0;
    constexpr double Hat808Freq2 = 5700.0;
    constexpr double Hat808Decay = 80.0;
    constexpr DWORD Hat808DurationMs = 40;

    // 808 Open Hat: metallic shimmer
    constexpr double OpenHat808Decay = 6.0;
    constexpr DWORD OpenHat808DurationMs = 400;

    // 808 Clap: layered bursts with reverb tail
    constexpr double Clap808BurstDecay = 60.0;
    constexpr double Clap808TailDecay = 8.0;
    constexpr double Clap808TailMix = 0.6;
    constexpr double Clap808Burst1Ms = 0.0;
    constexpr double Clap808Burst2Ms = 15.0;
    constexpr double Clap808Burst3Ms = 30.0;
    constexpr double Clap808Burst4Ms = 40.0;
    constexpr double Clap808TailStartMs = 50.0;
    constexpr DWORD Clap808DurationMs = 350;

    // Ride: bell-like metallic shimmer, higher and brighter than hat
    constexpr double RideFreq1 = 5500.0;
    constexpr double RideFreq2 = 8200.0;
    constexpr double RideFreq3 = 11000.0;
    constexpr double RideBellFreq = 2800.0;
    constexpr double RideDecay = 2.0;
    constexpr double RideBellMix = 0.5;
    constexpr DWORD RideDurationMs = 800;

    // Chip waveform constants
    constexpr double ChipPulseWidth = 0.25;
    constexpr double ChipNoiseMix = 0.15;

    // Rim: short sharp click
    constexpr double RimFreq = 800.0;
    constexpr double RimDecay = 100.0;
    constexpr DWORD RimDurationMs = 30;

    // Tom: pitched sine
    constexpr double TomFreq = 100.0;
    constexpr double TomSweepRange = 60.0;
    constexpr double TomSweepRate = 30.0;
    constexpr double TomDecay = 10.0;
    constexpr DWORD TomDurationMs = 250;

    // Sub-bass: sine + harmonics for audibility, uses note frequency
    constexpr double SubBassDecay = 2.5;
    constexpr double SubBassClickMultiplier = 3.0;
    constexpr double SubBassClickDecay = 30.0;
    constexpr double SubBassClickMix = 0.5;
    constexpr double SubBassOctaveMix = 0.6;
    constexpr DWORD SubBassDurationMs = 600;

    // Wub: LFO wobble sawtooth bass, uses note frequency
    constexpr double WubLfoRate = 3.0;
    constexpr double WubLfoDepth = 0.8;
    constexpr double WubDecay = 2.0;
    constexpr double WubHarmonicMix = 0.7;
    constexpr double WubOctaveMix = 0.8;
    constexpr DWORD WubDurationMs = 800;

    constexpr double MsToSeconds = 1000.0;

    constexpr uint32_t XorshiftSeed = 0x12345678;
    constexpr uint32_t XorshiftShift1 = 13;
    constexpr uint32_t XorshiftShift2 = 17;
    constexpr uint32_t XorshiftShift3 = 5;
    constexpr double NoiseNormalizer = static_cast<double>(INT32_MAX);

    DWORD MsToDurationSamples(DWORD ms)
    {
        return (Audio::SampleRate * ms) / 1000;
    }
}

Synthesizer::Synthesizer() : NoiseState(XorshiftSeed) {}

double Synthesizer::GenerateWaveformSample(WaveformType waveform, double phase) const
{
    switch (waveform)
    {
    case WaveformType::Sine:
        return std::sin(phase);
    case WaveformType::Square:
        return (phase < Audio::Pi) ? 1.0 : -1.0;
    case WaveformType::Sawtooth:
        return (phase / Audio::Pi) - 1.0;
    case WaveformType::Triangle:
        if (phase < Audio::Pi) { return (2.0 * phase / Audio::Pi) - 1.0; }
        return 3.0 - (2.0 * phase / Audio::Pi);
    case WaveformType::Sub:
    {
        const double fundamental = std::sin(phase);
        const double octaveUp = SubBassOctaveMix * std::sin(phase * 2.0);
        const double click = SubBassClickMix * std::sin(phase * SubBassClickMultiplier);
        return fundamental + octaveUp + click;
    }
    case WaveformType::Wub:
    {
        const double saw = (phase / Audio::Pi) - 1.0;
        const double fundamental = std::sin(phase);
        const double octaveUp = WubOctaveMix * std::sin(phase * 2.0);
        const double harmonics = WubHarmonicMix * std::sin(phase * 3.0);
        return saw * 0.5 + fundamental * 0.3 + octaveUp + harmonics;
    }
    case WaveformType::Chip:
    {
        const double pulse = (phase < Audio::Pi * 2.0 * ChipPulseWidth) ? 1.0 : -1.0;
        return pulse;
    }
    default:
        return 0.0;
    }
}

double Synthesizer::GenerateModulatedSample(WaveformType waveform, double baseFreqHz, uint32_t modulation, double phase, DWORD sampleOffset) const
{
    constexpr double CentsToRatio = 1200.0;
    const double t = static_cast<double>(sampleOffset) / static_cast<double>(Audio::SampleRate);

    // Vibrato: modulate frequency with LFO
    double freq = baseFreqHz;
    if (0 != (modulation & Mod::Vibrato))
    {
        const double vibratoLfo = std::sin(TwoPi * ModParams::VibratoRateHz * t);
        freq = baseFreqHz * std::pow(2.0, (ModParams::VibratoDepthSemitones * vibratoLfo) / SemitonesPerOctave);
    }

    // PWM: modulate pulse width for square waves
    double pwmWidth = 0.5;
    if (0 != (modulation & Mod::Pwm))
    {
        const double pwmLfo = std::sin(TwoPi * ModParams::PwmRateHz * t);
        pwmWidth = ModParams::PwmMinWidth + (ModParams::PwmMaxWidth - ModParams::PwmMinWidth) * (pwmLfo + 1.0) * 0.5;
    }

    double sample = 0.0;

    // Detune: layer multiple detuned voices
    if (0 != (modulation & Mod::Detune))
    {
        constexpr double DetuneSpread[] = { -1.0, 0.0, 1.0 };
        for (int v = 0; v < ModParams::DetuneVoices; ++v)
        {
            const double detuneRatio = std::pow(2.0, (DetuneSpread[v] * ModParams::DetuneAmountCents) / CentsToRatio);
            const double detunedFreq = freq * detuneRatio;
            const double detunePhaseStep = (TwoPi * detunedFreq) / static_cast<double>(Audio::SampleRate);
            const double detunePhase = std::fmod(detunePhaseStep * static_cast<double>(sampleOffset), TwoPi);

            if (WaveformType::Square == waveform && 0 != (modulation & Mod::Pwm))
            {
                sample += (detunePhase < TwoPi * pwmWidth) ? 1.0 : -1.0;
            }
            else
            {
                sample += GenerateWaveformSample(waveform, detunePhase);
            }
        }
        sample /= static_cast<double>(ModParams::DetuneVoices);
    }
    else
    {
        if (WaveformType::Square == waveform && 0 != (modulation & Mod::Pwm))
        {
            sample = (phase < TwoPi * pwmWidth) ? 1.0 : -1.0;
        }
        else
        {
            sample = GenerateWaveformSample(waveform, phase);
        }
    }

    // Tremolo: modulate amplitude with LFO
    if (0 != (modulation & Mod::Tremolo))
    {
        const double tremoloLfo = std::sin(TwoPi * ModParams::TremoloRateHz * t);
        const double tremoloFactor = 1.0 - ModParams::TremoloDepth * (tremoloLfo + 1.0) * 0.5;
        sample *= tremoloFactor;
    }

    // Distortion: overdrive and clip
    if (0 != (modulation & Mod::Distortion))
    {
        sample *= ModParams::DistortionGain;
        if (sample > ModParams::DistortionClip) { sample = ModParams::DistortionClip; }
        if (sample < -ModParams::DistortionClip) { sample = -ModParams::DistortionClip; }
        sample /= ModParams::DistortionClip;
    }

    // Bitcrush: quantize to fewer amplitude levels for lo-fi crunch
    if (0 != (modulation & Mod::Bitcrush))
    {
        sample = std::round(sample * ModParams::BitcrushLevels) / ModParams::BitcrushLevels;
    }

    return sample;
}

double Synthesizer::ApplyAdsr(double sample, DWORD sampleOffset, DWORD totalDuration) const
{
    const double attackSamples = Envelope::AttackMs * static_cast<double>(Audio::SampleRate) / 1000.0;
    const double decaySamples = Envelope::DecayMs * static_cast<double>(Audio::SampleRate) / 1000.0;
    const double releaseSamples = Envelope::ReleaseMs * static_cast<double>(Audio::SampleRate) / 1000.0;
    const double pos = static_cast<double>(sampleOffset);
    const double total = static_cast<double>(totalDuration);

    double envelope = 1.0;
    if (pos < attackSamples)
    {
        envelope = pos / attackSamples;
    }
    else if (pos < attackSamples + decaySamples)
    {
        const double decayProgress = (pos - attackSamples) / decaySamples;
        envelope = 1.0 - (1.0 - Envelope::SustainLevel) * decayProgress;
    }
    else if (pos > total - releaseSamples)
    {
        envelope = Envelope::SustainLevel * (total - pos) / releaseSamples;
        if (envelope < 0.0) { envelope = 0.0; }
    }
    else
    {
        envelope = Envelope::SustainLevel;
    }

    return sample * envelope;
}

double Synthesizer::GenerateNoiseSample()
{
    NoiseState ^= (NoiseState << XorshiftShift1);
    NoiseState ^= (NoiseState >> XorshiftShift2);
    NoiseState ^= (NoiseState << XorshiftShift3);
    return static_cast<double>(static_cast<int32_t>(NoiseState)) / NoiseNormalizer;
}

double Synthesizer::GenerateDrumSample(DrumType drum, DWORD sampleOffset)
{
    const double t = static_cast<double>(sampleOffset) / static_cast<double>(Audio::SampleRate);

    switch (drum)
    {
    case DrumType::Kick:
    {
        if (sampleOffset >= MsToDurationSamples(KickDurationMs)) { return 0.0; }
        const double freq = KickBaseFreq + KickSweepRange * std::exp(-KickSweepRate * t);
        const double envelope = std::exp(-KickDecayRate * t);
        return envelope * std::sin(TwoPi * freq * t);
    }
    case DrumType::Snare:
    {
        if (sampleOffset >= MsToDurationSamples(SnareDurationMs)) { return 0.0; }
        const double body = SnareBodyMix * std::exp(-SnareBodyDecay * t) * std::sin(TwoPi * SnareBodyFreq * t);
        const double noise = SnareNoiseMix * std::exp(-SnareNoiseDecay * t) * GenerateNoiseSample();
        return body + noise;
    }
    case DrumType::ClosedHat:
    {
        if (sampleOffset >= MsToDurationSamples(ClosedHatDurationMs)) { return 0.0; }
        return std::exp(-ClosedHatDecay * t) * GenerateNoiseSample();
    }
    case DrumType::OpenHat:
    {
        if (sampleOffset >= MsToDurationSamples(OpenHatDurationMs)) { return 0.0; }
        return std::exp(-OpenHatDecay * t) * GenerateNoiseSample();
    }
    case DrumType::Clap:
    {
        if (sampleOffset >= MsToDurationSamples(ClapDurationMs)) { return 0.0; }
        const double tMs = t * MsToSeconds;
        if (tMs < ClapBurst2Ms) { return std::exp(-ClapBurstDecay * (tMs - ClapBurst1Ms) / MsToSeconds) * GenerateNoiseSample(); }
        if (tMs < ClapBurst3Ms) { return std::exp(-ClapBurstDecay * (tMs - ClapBurst2Ms) / MsToSeconds) * GenerateNoiseSample(); }
        if (tMs < ClapTailStartMs) { return std::exp(-ClapBurstDecay * (tMs - ClapBurst3Ms) / MsToSeconds) * GenerateNoiseSample(); }
        return ClapTailMix * std::exp(-ClapTailDecay * (tMs - ClapTailStartMs) / MsToSeconds) * GenerateNoiseSample();
    }
    case DrumType::Kick808:
    {
        if (sampleOffset >= MsToDurationSamples(Kick808DurationMs)) { return 0.0; }
        const double freq = Kick808BaseFreq + Kick808SweepRange * std::exp(-Kick808SweepRate * t);
        const double envelope = std::exp(-Kick808DecayRate * t);
        return envelope * std::sin(TwoPi * freq * t);
    }
    case DrumType::Snare808:
    {
        if (sampleOffset >= MsToDurationSamples(Snare808DurationMs)) { return 0.0; }
        const double body = Snare808BodyMix * std::exp(-Snare808BodyDecay * t) * std::sin(TwoPi * Snare808BodyFreq * t);
        const double noise = Snare808NoiseMix * std::exp(-Snare808NoiseDecay * t) * GenerateNoiseSample();
        return body + noise;
    }
    case DrumType::Hat808:
    {
        if (sampleOffset >= MsToDurationSamples(Hat808DurationMs)) { return 0.0; }
        const double envelope = std::exp(-Hat808Decay * t);
        const double metallic = std::sin(TwoPi * Hat808Freq1 * t) * 0.5 + std::sin(TwoPi * Hat808Freq2 * t) * 0.5;
        return envelope * (metallic * 0.6 + GenerateNoiseSample() * 0.4);
    }
    case DrumType::OpenHat808:
    {
        if (sampleOffset >= MsToDurationSamples(OpenHat808DurationMs)) { return 0.0; }
        const double envelope = std::exp(-OpenHat808Decay * t);
        const double metallic = std::sin(TwoPi * Hat808Freq1 * t) * 0.5 + std::sin(TwoPi * Hat808Freq2 * t) * 0.5;
        return envelope * (metallic * 0.6 + GenerateNoiseSample() * 0.4);
    }
    case DrumType::Clap808:
    {
        if (sampleOffset >= MsToDurationSamples(Clap808DurationMs)) { return 0.0; }
        const double tMs = t * MsToSeconds;
        if (tMs < Clap808Burst2Ms) { return std::exp(-Clap808BurstDecay * (tMs - Clap808Burst1Ms) / MsToSeconds) * GenerateNoiseSample(); }
        if (tMs < Clap808Burst3Ms) { return std::exp(-Clap808BurstDecay * (tMs - Clap808Burst2Ms) / MsToSeconds) * GenerateNoiseSample(); }
        if (tMs < Clap808Burst4Ms) { return std::exp(-Clap808BurstDecay * (tMs - Clap808Burst3Ms) / MsToSeconds) * GenerateNoiseSample(); }
        if (tMs < Clap808TailStartMs) { return std::exp(-Clap808BurstDecay * (tMs - Clap808Burst4Ms) / MsToSeconds) * GenerateNoiseSample(); }
        return Clap808TailMix * std::exp(-Clap808TailDecay * (tMs - Clap808TailStartMs) / MsToSeconds) * GenerateNoiseSample();
    }
    case DrumType::Ride:
    {
        if (sampleOffset >= MsToDurationSamples(RideDurationMs)) { return 0.0; }
        const double envelope = std::exp(-RideDecay * t);
        const double bell = RideBellMix * std::sin(TwoPi * RideBellFreq * t) * std::exp(-4.0 * t);
        const double shimmer = std::sin(TwoPi * RideFreq1 * t) * 0.25 + std::sin(TwoPi * RideFreq2 * t) * 0.2 + std::sin(TwoPi * RideFreq3 * t) * 0.15;
        return envelope * (bell + shimmer + GenerateNoiseSample() * 0.15);
    }
    case DrumType::Rim:
    {
        if (sampleOffset >= MsToDurationSamples(RimDurationMs)) { return 0.0; }
        const double envelope = std::exp(-RimDecay * t);
        return envelope * std::sin(TwoPi * RimFreq * t);
    }
    case DrumType::Tom:
    {
        if (sampleOffset >= MsToDurationSamples(TomDurationMs)) { return 0.0; }
        const double freq = TomFreq + TomSweepRange * std::exp(-TomSweepRate * t);
        const double envelope = std::exp(-TomDecay * t);
        return envelope * std::sin(TwoPi * freq * t);
    }
    default:
        return 0.0;
    }
}

DWORD Synthesizer::GetDrumDurationSamples(DrumType drum)
{
    switch (drum)
    {
    case DrumType::Kick: return MsToDurationSamples(KickDurationMs);
    case DrumType::Snare: return MsToDurationSamples(SnareDurationMs);
    case DrumType::ClosedHat: return MsToDurationSamples(ClosedHatDurationMs);
    case DrumType::OpenHat: return MsToDurationSamples(OpenHatDurationMs);
    case DrumType::Clap: return MsToDurationSamples(ClapDurationMs);
    case DrumType::Kick808: return MsToDurationSamples(Kick808DurationMs);
    case DrumType::Snare808: return MsToDurationSamples(Snare808DurationMs);
    case DrumType::Hat808: return MsToDurationSamples(Hat808DurationMs);
    case DrumType::OpenHat808: return MsToDurationSamples(OpenHat808DurationMs);
    case DrumType::Clap808: return MsToDurationSamples(Clap808DurationMs);
    case DrumType::Ride: return MsToDurationSamples(RideDurationMs);
    case DrumType::Rim: return MsToDurationSamples(RimDurationMs);
    case DrumType::Tom: return MsToDurationSamples(TomDurationMs);
    default: return 0;
    }
}
