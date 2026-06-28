#include "Pattern.h"
#include <algorithm>

Pattern::Pattern() : CurrentStep(0), pRows(std::make_unique<std::vector<InstrumentRow>>()), Bpm(Sequencer::DefaultBpm)
{
    pRows->reserve(static_cast<size_t>(Sequencer::MaxRows));
}

void Pattern::ToggleStep(int row, int column)
{
    if (row >= 0 && row < static_cast<int>(pRows->size()) && column >= 0 && column < Sequencer::StepCount)
    {
        auto& steps = (*pRows)[static_cast<size_t>(row)].Steps;
        steps[static_cast<size_t>(column)] = !steps[static_cast<size_t>(column)];
    }
}

bool Pattern::GetStep(int row, int column) const
{
    if (row >= 0 && row < static_cast<int>(pRows->size()) && column >= 0 && column < Sequencer::StepCount)
    {
        return (*pRows)[static_cast<size_t>(row)].Steps[static_cast<size_t>(column)];
    }
    return false;
}

int Pattern::GetRowCount() const
{
    return static_cast<int>(pRows->size());
}

const InstrumentRow& Pattern::GetRow(int index) const
{
    return (*pRows)[static_cast<size_t>(index)];
}

void Pattern::SetLoop(const std::string& key, InstrumentType type, WaveformType waveform, DrumType drum, double freqHz, uint32_t modulation, const std::vector<int>& steps)
{
    constexpr double DefaultLoopVolume = 1.0;
    constexpr int StepIndexOffset = 1;

    for (auto& row : *pRows)
    {
        if (key == row.Name)
        {
            row.Steps.fill(false);
            for (const int step : steps)
            {
                const int idx = step - StepIndexOffset;
                if (idx >= 0 && idx < Sequencer::StepCount)
                {
                    row.Steps[static_cast<size_t>(idx)] = true;
                }
            }
            return;
        }
    }

    if (static_cast<int>(pRows->size()) >= Sequencer::MaxRows)
    {
        return;
    }

    InstrumentRow row;
    row.Name = key;
    row.Type = type;
    row.Waveform = waveform;
    row.Drum = drum;
    row.FrequencyHz = freqHz;
    row.Volume = DefaultLoopVolume;
    row.Modulation = modulation;
    row.Steps.fill(false);
    for (const int step : steps)
    {
        const int idx = step - StepIndexOffset;
        if (idx >= 0 && idx < Sequencer::StepCount)
        {
            row.Steps[static_cast<size_t>(idx)] = true;
        }
    }
    pRows->push_back(row);
}

void Pattern::ClearLoop(const std::string& key)
{
    auto it = std::remove_if(pRows->begin(), pRows->end(), [&key](const InstrumentRow& row) { return key == row.Name; });
    pRows->erase(it, pRows->end());
}

void Pattern::ClearAllLoops()
{
    pRows->clear();
}

std::unique_ptr<std::vector<std::pair<std::string, std::array<bool, Sequencer::StepCount>>>> Pattern::GetActiveLoops() const
{
    auto pLoops = std::make_unique<std::vector<std::pair<std::string, std::array<bool, Sequencer::StepCount>>>>();
    for (const auto& row : *pRows)
    {
        pLoops->push_back({ row.Name, row.Steps });
    }
    return pLoops;
}

int Pattern::GetBpm() const
{
    return Bpm.load();
}

void Pattern::SetBpm(int bpm)
{
    if (bpm < Sequencer::MinBpm) { bpm = Sequencer::MinBpm; }
    if (bpm > Sequencer::MaxBpm) { bpm = Sequencer::MaxBpm; }
    Bpm.store(bpm);
}

DWORD Pattern::GetStepDurationSamples() const
{
    const int bpm = Bpm.load();
    return (Audio::SampleRate * Sequencer::SecondsPerMinute) / (static_cast<DWORD>(bpm) * Sequencer::StepsPerBeat);
}
