#include "NoteFilters.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
    constexpr double MinAudibleHz = 30.0;
    constexpr double MaxAudibleHz = 16000.0;
    constexpr double OctaveRatio = 2.0;
    constexpr int MinOctaveShift = -4;
    constexpr int MaxOctaveShift = 4;

    int ClampOctaveShift(int shift)
    {
        if (MinOctaveShift > shift)
        {
            return MinOctaveShift;
        }
        if (MaxOctaveShift < shift)
        {
            return MaxOctaveShift;
        }

        return shift;
    }

    double OctaveMultiplier(int shift)
    {
        return std::pow(OctaveRatio, static_cast<double>(shift));
    }

    bool PassesAudibleClamp(double freqHz, bool clampEnabled)
    {
        if (false == clampEnabled)
        {
            return true;
        }

        return (MinAudibleHz <= freqHz && MaxAudibleHz >= freqHz);
    }

    struct MergeContext
    {
        const AppState* pState;
        bool ApplyVolume;
    };

    void AppendTrack(std::vector<ConvertedNote>& dest, const TrackCacheEntry& entry, const MergeContext& context)
    {
        if (nullptr == entry.pNotes || true == entry.pNotes->empty())
        {
            return;
        }

        const AppState* pState = context.pState;
        const double freqMul = OctaveMultiplier(ClampOctaveShift(pState->OctaveShift));
        const double volMul = (true == context.ApplyVolume) ? entry.Volume : 1.0;
        if (0.0 >= volMul)
        {
            return;
        }

        const std::vector<ConvertedNote>* pSource = entry.pNotes.get();

        std::unique_ptr<std::vector<ConvertedNote>> pMono;
        if (true == pState->FilterMonophonic)
        {
            pMono = NoteConverter::ApplySkylineFilter(*pSource);
            pSource = pMono.get();
        }

        for (const auto& note : *pSource)
        {
            ConvertedNote shaped = note;
            shaped.FrequencyHz *= freqMul;

            if (false == PassesAudibleClamp(shaped.FrequencyHz, pState->FilterClampAudible))
            {
                continue;
            }

            shaped.Velocity *= volMul;
            dest.push_back(shaped);
        }
    }
}

namespace NoteFilters
{
    std::unique_ptr<std::vector<ConvertedNote>> BuildMergedNotes(const AppState* pState, bool applyVolume)
    {
        auto pAll = std::make_unique<std::vector<ConvertedNote>>();
        if (nullptr == pState || nullptr == pState->pTrackCache)
        {
            return pAll;
        }

        MergeContext context;
        context.pState = pState;
        context.ApplyVolume = applyVolume;

        for (const auto& entry : *pState->pTrackCache)
        {
            if (false == entry.IsSelected)
            {
                continue;
            }
            AppendTrack(*pAll, entry, context);
        }

        std::sort(pAll->begin(), pAll->end(), [](const ConvertedNote& a, const ConvertedNote& b)
            {
                if (a.StartMs != b.StartMs)
                {
                    return a.StartMs < b.StartMs;
                }
                return a.FrequencyHz < b.FrequencyHz;
            });

        return pAll;
    }
}
