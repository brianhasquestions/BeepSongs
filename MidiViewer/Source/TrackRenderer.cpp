#include "TrackRenderer.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr int PianoKeyWidth = 40;
    constexpr int TimeAxisHeight = 18;
    constexpr int NoteMinHeight = 2;
    constexpr int NoteRounding = 2;
    constexpr int MinPitchRangeSemitones = 12;
    constexpr int PitchPaddingSemitones = 2;
    constexpr int SemitonesPerOctave = 12;
    constexpr int MiddleCMidi = 60;
    constexpr int MidiRefNote = 69;
    constexpr double MidiRefFreq = 440.0;
    constexpr double OctaveRatio = 2.0;
    constexpr double RoundingBias = 0.5;
    constexpr double MinZoom = 1.0;
    constexpr unsigned int TimeLabelStepMs = 1000;
    constexpr int TimeLabelMinPixelGap = 60;
    constexpr unsigned int MsPerSecond = 1000;
    constexpr int TimeStepScale = 2;
    constexpr int TimeLabelPadX = 2;
    constexpr int TimeLabelBaselineOffset = 14;
    constexpr int EmptyPromptMargin = 16;
    constexpr COLORREF BackgroundColor = RGB(24, 24, 28);
    constexpr COLORREF GridColor = RGB(48, 48, 56);
    constexpr COLORREF OctaveColor = RGB(80, 80, 96);
    constexpr COLORREF BeatLineColor = RGB(60, 60, 72);
    constexpr COLORREF AxisTextColor = RGB(200, 200, 210);
    constexpr COLORREF PianoBlackKeyColor = RGB(18, 18, 22);
    constexpr COLORREF PianoWhiteKeyColor = RGB(60, 60, 70);
    constexpr COLORREF CursorColor = RGB(255, 240, 120);
    constexpr int CursorPenWidth = 2;
    constexpr unsigned int NoCursor = 0xFFFFFFFFu;

    struct RollCtx
    {
        HDC hdc;
        RECT rect;
        int lowest;
        int highest;
    };

    struct TimeMap
    {
        double pixelsPerMs;
        unsigned int scrollMs;
        int leftX;
        int width;
    };

    bool IsBlackKey(int noteNumber)
    {
        const int mod = ((noteNumber % SemitonesPerOctave) + SemitonesPerOctave) % SemitonesPerOctave;
        return (1 == mod || 3 == mod || 6 == mod || 8 == mod || 10 == mod);
    }

    void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color)
    {
        const HBRUSH hBrush = CreateSolidBrush(color);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);
    }

    int ComputePitchY(int noteNumber, const RollCtx& ctx)
    {
        const int span = (ctx.highest - ctx.lowest) + 1;
        const int rollHeight = ctx.rect.bottom - ctx.rect.top;
        const int offset = ctx.highest - noteNumber;
        return (offset * rollHeight) / span;
    }

    int RowHeight(const RollCtx& ctx)
    {
        const int span = (ctx.highest - ctx.lowest) + 1;
        const int rollHeight = ctx.rect.bottom - ctx.rect.top;
        return std::max(1, rollHeight / span);
    }

    int MsToX(unsigned int ms, const TimeMap& map)
    {
        const double delta = static_cast<double>(ms) - static_cast<double>(map.scrollMs);
        return map.leftX + static_cast<int>(delta * map.pixelsPerMs);
    }

    void DrawPianoGutter(const RollCtx& ctx)
    {
        FillSolidRect(ctx.hdc, ctx.rect, PianoBlackKeyColor);

        const int span = (ctx.highest - ctx.lowest) + 1;
        if (0 >= span)
        {
            return;
        }

        const int rowHeight = RowHeight(ctx);

        SetBkMode(ctx.hdc, TRANSPARENT);
        SetTextColor(ctx.hdc, AxisTextColor);

        for (int note = ctx.lowest; note <= ctx.highest; ++note)
        {
            const int yTop = ctx.rect.top + ComputePitchY(note, ctx);
            RECT keyRect = { ctx.rect.left, yTop, ctx.rect.right, yTop + rowHeight };

            if (false == IsBlackKey(note))
            {
                FillSolidRect(ctx.hdc, keyRect, PianoWhiteKeyColor);
            }

            if (0 == (note % SemitonesPerOctave))
            {
                wchar_t label[8] = { 0 };
                const int octave = (note / SemitonesPerOctave) - 1;
                wsprintfW(label, L"C%d", octave);
                TextOutW(ctx.hdc, ctx.rect.left + 4, yTop, label, lstrlenW(label));
            }
        }
    }

    void DrawPitchGrid(const RollCtx& ctx)
    {
        const int span = (ctx.highest - ctx.lowest) + 1;
        if (0 >= span)
        {
            return;
        }

        const HPEN hOctavePen = CreatePen(PS_SOLID, 1, OctaveColor);
        const HPEN hGridPen = CreatePen(PS_SOLID, 1, GridColor);

        for (int note = ctx.lowest; note <= ctx.highest; ++note)
        {
            const int y = ctx.rect.top + ComputePitchY(note, ctx);
            const HPEN hPen = (0 == (note % SemitonesPerOctave)) ? hOctavePen : hGridPen;
            const HGDIOBJ hOld = SelectObject(ctx.hdc, hPen);
            MoveToEx(ctx.hdc, ctx.rect.left, y, nullptr);
            LineTo(ctx.hdc, ctx.rect.right, y);
            SelectObject(ctx.hdc, hOld);
        }

        DeleteObject(hOctavePen);
        DeleteObject(hGridPen);
    }

    unsigned int ChooseTimeStepMs(double pixelsPerMs)
    {
        unsigned int step = TimeLabelStepMs;
        while (static_cast<double>(TimeLabelMinPixelGap) > step * pixelsPerMs)
        {
            step *= TimeStepScale;
        }
        while (static_cast<double>(TimeLabelMinPixelGap * TimeStepScale) < (step / TimeStepScale) * pixelsPerMs && 1 < step)
        {
            step /= TimeStepScale;
            if (0 == step)
            {
                step = 1;
                break;
            }
        }
        return step;
    }

    void DrawTimeGrid(const RollCtx& ctx, const TimeMap& map, unsigned int totalMs)
    {
        const HPEN hPen = CreatePen(PS_SOLID, 1, BeatLineColor);
        const HGDIOBJ hOldPen = SelectObject(ctx.hdc, hPen);

        SetBkMode(ctx.hdc, TRANSPARENT);
        SetTextColor(ctx.hdc, AxisTextColor);

        const unsigned int stepMs = ChooseTimeStepMs(map.pixelsPerMs);
        if (0 == stepMs)
        {
            SelectObject(ctx.hdc, hOldPen);
            DeleteObject(hPen);
            return;
        }

        const unsigned int firstTick = (map.scrollMs / stepMs) * stepMs;

        for (unsigned int t = firstTick; t <= totalMs; t += stepMs)
        {
            const int x = MsToX(t, map);
            if (x < ctx.rect.left)
            {
                continue;
            }
            if (x > ctx.rect.right)
            {
                break;
            }

            MoveToEx(ctx.hdc, x, ctx.rect.top, nullptr);
            LineTo(ctx.hdc, x, ctx.rect.bottom);

            wchar_t label[16] = { 0 };
            wsprintfW(label, L"%u.%03us", t / MsPerSecond, t % MsPerSecond);
            TextOutW(ctx.hdc, x + TimeLabelPadX, ctx.rect.bottom - TimeLabelBaselineOffset, label, lstrlenW(label));
        }

        SelectObject(ctx.hdc, hOldPen);
        DeleteObject(hPen);
    }

    int FrequencyToMidi(double freqHz)
    {
        const double ratio = freqHz / MidiRefFreq;
        const double semis = static_cast<double>(SemitonesPerOctave) * (std::log(ratio) / std::log(OctaveRatio));
        return static_cast<int>(semis + MidiRefNote + RoundingBias);
    }

    struct DrawNotesArgs
    {
        const RollCtx* pCtx;
        const TimeMap* pMap;
        const TrackCacheEntry* pEntry;
    };

    void DrawTrackNotes(const DrawNotesArgs& args)
    {
        const TrackCacheEntry& entry = *args.pEntry;
        if (false == entry.IsSelected || nullptr == entry.pNotes || true == entry.pNotes->empty())
        {
            return;
        }

        const RollCtx& ctx = *args.pCtx;
        const TimeMap& map = *args.pMap;
        const int rowHeight = std::max(NoteMinHeight, RowHeight(ctx));

        const HBRUSH hBrush = CreateSolidBrush(static_cast<COLORREF>(entry.ColorRef));
        const HGDIOBJ hOldBrush = SelectObject(ctx.hdc, hBrush);

        for (const auto& note : *entry.pNotes)
        {
            if (0.0 >= note.FrequencyHz)
            {
                continue;
            }

            const int xStart = MsToX(note.StartMs, map);
            const int xEnd = MsToX(note.StartMs + note.DurationMs, map);

            if (xEnd < ctx.rect.left || xStart > ctx.rect.right)
            {
                continue;
            }

            const int midi = FrequencyToMidi(note.FrequencyHz);
            if (midi < ctx.lowest || midi > ctx.highest)
            {
                continue;
            }

            const int yTop = ctx.rect.top + ComputePitchY(midi, ctx);
            const int rectLeft = static_cast<int>(ctx.rect.left);
            const int rectRight = static_cast<int>(ctx.rect.right);
            const int clampedLeft = std::max(xStart, rectLeft);
            const int minRight = std::max(xEnd, clampedLeft + 1);
            const int clampedRight = std::min(minRight, rectRight);

            RoundRect(ctx.hdc, clampedLeft, yTop, clampedRight, yTop + rowHeight, NoteRounding, NoteRounding);
        }

        SelectObject(ctx.hdc, hOldBrush);
        DeleteObject(hBrush);
    }

    void DrawEmptyPrompt(HDC hdc, const RECT& clientRect)
    {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, AxisTextColor);
        const wchar_t* prompt = L"Open a .mid file via File > Open...";
        TextOutW(hdc, clientRect.left + EmptyPromptMargin, clientRect.top + EmptyPromptMargin, prompt, lstrlenW(prompt));
    }

    bool HasAnySelectedNotes(const AppState* pState)
    {
        for (const auto& entry : *pState->pTrackCache)
        {
            if (true == entry.IsSelected && nullptr != entry.pNotes && false == entry.pNotes->empty())
            {
                return true;
            }
        }
        return false;
    }
}

namespace TrackRenderer
{
    void Paint(HDC hdc, const RECT& clientRect, const AppState* pState)
    {
        FillSolidRect(hdc, clientRect, BackgroundColor);

        if (nullptr == pState || nullptr == pState->pTrackCache || true == pState->pTrackCache->empty() || false == HasAnySelectedNotes(pState))
        {
            DrawEmptyPrompt(hdc, clientRect);
            return;
        }

        int lowest = static_cast<int>(pState->LowestNote);
        int highest = static_cast<int>(pState->HighestNote);
        if (MinPitchRangeSemitones > highest - lowest)
        {
            lowest = std::min(lowest, MiddleCMidi - (MinPitchRangeSemitones / 2));
            highest = std::max(highest, MiddleCMidi + (MinPitchRangeSemitones / 2));
        }
        lowest -= PitchPaddingSemitones;
        highest += PitchPaddingSemitones;
        if (0 > lowest)
        {
            lowest = 0;
        }

        RollCtx gutterCtx;
        gutterCtx.hdc = hdc;
        gutterCtx.rect = { clientRect.left, clientRect.top, clientRect.left + PianoKeyWidth, clientRect.bottom - TimeAxisHeight };
        gutterCtx.lowest = lowest;
        gutterCtx.highest = highest;

        RollCtx rollCtx;
        rollCtx.hdc = hdc;
        rollCtx.rect = { clientRect.left + PianoKeyWidth, clientRect.top, clientRect.right, clientRect.bottom - TimeAxisHeight };
        rollCtx.lowest = lowest;
        rollCtx.highest = highest;

        const int rollWidth = rollCtx.rect.right - rollCtx.rect.left;
        const unsigned int totalMs = (0 == pState->TotalDurationMs) ? 1 : pState->TotalDurationMs;

        double zoom = pState->ZoomX;
        if (MinZoom > zoom)
        {
            zoom = MinZoom;
        }
        const double basePpm = static_cast<double>(rollWidth) / static_cast<double>(totalMs);

        TimeMap map;
        map.pixelsPerMs = basePpm * zoom;
        map.scrollMs = pState->ScrollXMs;
        map.leftX = rollCtx.rect.left;
        map.width = rollWidth;

        DrawPianoGutter(gutterCtx);
        DrawPitchGrid(rollCtx);
        DrawTimeGrid(rollCtx, map, totalMs);

        DrawNotesArgs args;
        args.pCtx = &rollCtx;
        args.pMap = &map;

        for (const auto& entry : *pState->pTrackCache)
        {
            args.pEntry = &entry;
            DrawTrackNotes(args);
        }

        if (NoCursor != pState->PlaybackCursorMs)
        {
            const int cx = MsToX(pState->PlaybackCursorMs, map);
            if (cx >= rollCtx.rect.left && cx <= rollCtx.rect.right)
            {
                const HPEN hCursorPen = CreatePen(PS_SOLID, CursorPenWidth, CursorColor);
                const HGDIOBJ hOld = SelectObject(hdc, hCursorPen);
                MoveToEx(hdc, cx, rollCtx.rect.top, nullptr);
                LineTo(hdc, cx, rollCtx.rect.bottom);
                SelectObject(hdc, hOld);
                DeleteObject(hCursorPen);
            }
        }
    }
}
