#include "MainWindow.h"

#include <Windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "AppState.h"
#include "BeepSongsInstaller.h"
#include "ExportDialog.h"
#include "MidiParser.h"
#include "MidiTypes.h"
#include "NoteConverter.h"
#include "NoteFilters.h"
#include "Resource.h"
#include "TrackPlayer.h"
#include "TrackRenderer.h"

namespace
{
    constexpr wchar_t MainClassName[] = L"MidiViewerMainClass";
    constexpr wchar_t PianoRollClassName[] = L"MidiViewerPianoRoll";
    constexpr wchar_t WindowTitle[] = L"MidiViewer - BeepSongs";

    constexpr int InitialWidth = 1280;
    constexpr int InitialHeight = 840;
    constexpr int TrackListWidth = 300;
    constexpr int ButtonBarHeight = 34;
    constexpr int StatusBarHeight = 22;
    constexpr int Padding = 6;
    constexpr int ButtonWidth = 130;
    constexpr int MaxFilePathChars = 1024;
    constexpr int StatusTextMax = 512;
    constexpr double DefaultVelocityScale = 1.0;
    constexpr unsigned char InitialLowestNote = 127;
    constexpr unsigned char InitialHighestNote = 0;
    constexpr unsigned char FallbackLowestNote = 60;
    constexpr unsigned char FallbackHighestNote = 72;
    constexpr double MinZoomX = 1.0;
    constexpr double MaxZoomX = 2000.0;
    constexpr double ZoomStep = 1.25;
    constexpr int WheelScrollDivisor = 4;
    constexpr int WheelDeltaStd = 120;
    constexpr UINT_PTR PlaybackTimerId = 1;
    constexpr UINT PlaybackTimerIntervalMs = 30;
    constexpr unsigned int NoPlaybackCursor = 0xFFFFFFFFu;
    constexpr int ColIndexTrackNumber = 0;
    constexpr int ColIndexNoteCount = 1;
    constexpr int ColIndexInstrument = 2;
    constexpr int ColIndexVolume = 3;
    constexpr double DefaultVolume = 1.0;
    constexpr double MaxVolume = 2.0;
    constexpr double MinVolume = 0.0;
    constexpr double VolumeStep = 0.1;
    constexpr int PercentScale = 100;
    constexpr int MinOctave = -4;
    constexpr int MaxOctave = 4;
    constexpr int ColWidthTrackNumber = 36;
    constexpr int ColWidthNoteCount = 60;
    constexpr int ColWidthInstrument = 140;
    constexpr int ColWidthVolume = 56;
    constexpr int SmallTextBufferChars = 16;
    constexpr double VolumePercentRoundingBias = 0.5;
    constexpr int ScrollLineStepDivisor = 8;
    constexpr int StateImageShift = 12;
    constexpr UINT CheckedStateImage = 2;
    HMENU g_hFilterMenu = nullptr;

    constexpr COLORREF TrackPalette[] =
    {
        RGB(255, 107, 107), RGB( 78, 205, 196), RGB(255, 206,  84),
        RGB(162, 155, 254), RGB(255, 159,  64), RGB( 46, 204, 113),
        RGB( 52, 152, 219), RGB(231,  76,  60), RGB(155,  89, 182),
        RGB(241, 196,  15), RGB( 26, 188, 156), RGB(230, 126,  34)
    };
    constexpr int TrackPaletteSize = static_cast<int>(sizeof(TrackPalette) / sizeof(TrackPalette[0]));

    AppState g_state;
    HWND g_hMain = nullptr;
    HWND g_hTrackList = nullptr;
    HWND g_hPianoRoll = nullptr;
    HWND g_hStatus = nullptr;
    HWND g_hPlayBtn = nullptr;
    HWND g_hStopBtn = nullptr;
    HINSTANCE g_hInstance = nullptr;

    std::string WideToUtf8(const wchar_t* pWide)
    {
        if (nullptr == pWide || 0 == *pWide)
        {
            return std::string();
        }
        const int needed = WideCharToMultiByte(CP_UTF8, 0, pWide, -1, nullptr, 0, nullptr, nullptr);
        if (1 >= needed)
        {
            return std::string();
        }
        std::string out(static_cast<size_t>(needed - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, pWide, -1, &out[0], needed, nullptr, nullptr);
        return out;
    }

    std::unique_ptr<std::wstring> Utf8ToWide(const std::string& text)
    {
        auto pOut = std::make_unique<std::wstring>();
        if (true == text.empty())
        {
            return pOut;
        }
        const int srcLen = static_cast<int>(text.size());
        const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), srcLen, nullptr, 0);
        if (0 >= needed)
        {
            return pOut;
        }
        pOut->resize(static_cast<size_t>(needed));
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), srcLen, &(*pOut)[0], needed);
        return pOut;
    }

    void SetStatusText(const wchar_t* pText)
    {
        if (nullptr == g_hStatus || nullptr == pText)
        {
            return;
        }
        SendMessageW(g_hStatus, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(pText));
    }

    HMENU BuildMainMenu()
    {
        const HMENU hMenu = CreateMenu();
        const HMENU hFile = CreatePopupMenu();
        AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN, L"&Open MIDI...\tCtrl+O");
        AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"E&xit");
        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFile), L"&File");

        const HMENU hExport = CreatePopupMenu();
        AppendMenuW(hExport, MF_STRING, IDM_EXPORT_INSTALL, L"&Install to BeepSongs...");
        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hExport), L"&Export");

        const HMENU hPlay = CreatePopupMenu();
        AppendMenuW(hPlay, MF_STRING, IDM_PLAY_SELECTED, L"&Play Checked Tracks\tSpace");
        AppendMenuW(hPlay, MF_STRING, IDM_PLAY_STOP, L"&Stop");
        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hPlay), L"&Play");

        g_hFilterMenu = CreatePopupMenu();
        AppendMenuW(g_hFilterMenu, MF_STRING, IDM_FILTER_MONOPHONIC, L"&Monophonic (Skyline) Per Track");
        AppendMenuW(g_hFilterMenu, MF_STRING, IDM_FILTER_CLAMP, L"&Clamp To Audible Range");
        AppendMenuW(g_hFilterMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(g_hFilterMenu, MF_STRING, IDM_FILTER_OCTAVE_UP, L"Transpose &Up One Octave\tCtrl++");
        AppendMenuW(g_hFilterMenu, MF_STRING, IDM_FILTER_OCTAVE_DOWN, L"Transpose &Down One Octave\tCtrl+-");
        AppendMenuW(g_hFilterMenu, MF_STRING, IDM_FILTER_OCTAVE_RESET, L"&Reset Transpose");
        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(g_hFilterMenu), L"&Filters");

        const HMENU hHelp = CreatePopupMenu();
        AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"&About...");
        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelp), L"&Help");

        return hMenu;
    }

    void InsertTrackListColumns()
    {
        LVCOLUMNW col = { 0 };
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

        col.pszText = const_cast<LPWSTR>(L"#");
        col.cx = ColWidthTrackNumber;
        col.iSubItem = ColIndexTrackNumber;
        ListView_InsertColumn(g_hTrackList, ColIndexTrackNumber, &col);

        col.pszText = const_cast<LPWSTR>(L"Notes");
        col.cx = ColWidthNoteCount;
        col.iSubItem = ColIndexNoteCount;
        ListView_InsertColumn(g_hTrackList, ColIndexNoteCount, &col);

        col.pszText = const_cast<LPWSTR>(L"Instrument");
        col.cx = ColWidthInstrument;
        col.iSubItem = ColIndexInstrument;
        ListView_InsertColumn(g_hTrackList, ColIndexInstrument, &col);

        col.pszText = const_cast<LPWSTR>(L"Vol");
        col.cx = ColWidthVolume;
        col.iSubItem = ColIndexVolume;
        ListView_InsertColumn(g_hTrackList, ColIndexVolume, &col);
    }

    void CreateChildren()
    {
        g_hTrackList = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
            0, 0, 0, 0, g_hMain, reinterpret_cast<HMENU>(IDC_TRACK_LIST), g_hInstance, nullptr);
        ListView_SetExtendedListViewStyle(g_hTrackList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        InsertTrackListColumns();

        g_hPianoRoll = CreateWindowExW(WS_EX_CLIENTEDGE, PianoRollClassName, L"",
            WS_CHILD | WS_VISIBLE | WS_HSCROLL,
            0, 0, 0, 0, g_hMain, reinterpret_cast<HMENU>(IDC_PIANO_ROLL), g_hInstance, nullptr);

        g_hPlayBtn = CreateWindowExW(0, L"BUTTON", L"Play Checked",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, g_hMain, reinterpret_cast<HMENU>(IDC_PLAY_BUTTON), g_hInstance, nullptr);

        g_hStopBtn = CreateWindowExW(0, L"BUTTON", L"Stop",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, g_hMain, reinterpret_cast<HMENU>(IDC_STOP_BUTTON), g_hInstance, nullptr);

        g_hStatus = CreateWindowExW(0, L"STATIC", L"Ready.",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            0, 0, 0, 0, g_hMain, reinterpret_cast<HMENU>(IDC_STATUS_BAR), g_hInstance, nullptr);
    }

    void LayoutChildren(int width, int height)
    {
        const int topBarY = 0;
        const int listX = 0;
        const int listY = ButtonBarHeight + Padding;
        const int listH = height - listY - StatusBarHeight - Padding;

        const int rightX = TrackListWidth + Padding;
        const int rightW = width - rightX;
        const int rollY = listY;
        const int rollH = listH;

        MoveWindow(g_hPlayBtn, Padding + 0 * (ButtonWidth + Padding), topBarY + Padding, ButtonWidth, ButtonBarHeight - Padding, TRUE);
        MoveWindow(g_hStopBtn, Padding + 1 * (ButtonWidth + Padding), topBarY + Padding, ButtonWidth, ButtonBarHeight - Padding, TRUE);

        MoveWindow(g_hTrackList, listX, listY, TrackListWidth, listH, TRUE);
        MoveWindow(g_hPianoRoll, rightX, rollY, rightW - Padding, rollH, TRUE);
        MoveWindow(g_hStatus, 0, height - StatusBarHeight, width, StatusBarHeight, TRUE);
    }

    void ClearAppState()
    {
        TrackPlayer::Stop();
        if (nullptr != g_hMain)
        {
            KillTimer(g_hMain, PlaybackTimerId);
        }
        g_state.pMidiFile.reset();
        g_state.pTempoMap.reset();
        g_state.pTrackCache.reset();
        g_state.pCurrentFilePath.reset();
        g_state.TotalDurationMs = 0;
        g_state.LowestNote = InitialLowestNote;
        g_state.HighestNote = InitialHighestNote;
        g_state.ZoomX = MinZoomX;
        g_state.ScrollXMs = 0;
        g_state.PlaybackCursorMs = NoPlaybackCursor;
    }

    void UpdateFilterMenuState()
    {
        if (nullptr == g_hFilterMenu)
        {
            return;
        }
        CheckMenuItem(g_hFilterMenu, IDM_FILTER_MONOPHONIC, MF_BYCOMMAND | (g_state.FilterMonophonic ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(g_hFilterMenu, IDM_FILTER_CLAMP, MF_BYCOMMAND | (g_state.FilterClampAudible ? MF_CHECKED : MF_UNCHECKED));
    }

    void RecomputeSelectedPitchRange()
    {
        g_state.LowestNote = InitialLowestNote;
        g_state.HighestNote = InitialHighestNote;

        if (nullptr == g_state.pTrackCache)
        {
            return;
        }

        for (const auto& entry : *g_state.pTrackCache)
        {
            if (false == entry.IsSelected || nullptr == entry.pNotes || true == entry.pNotes->empty())
            {
                continue;
            }
            if (entry.LowestNote < g_state.LowestNote)
            {
                g_state.LowestNote = entry.LowestNote;
            }
            if (entry.HighestNote > g_state.HighestNote)
            {
                g_state.HighestNote = entry.HighestNote;
            }
        }

        if (g_state.LowestNote > g_state.HighestNote)
        {
            g_state.LowestNote = FallbackLowestNote;
            g_state.HighestNote = FallbackHighestNote;
        }
    }

    void UpdateVolumeCell(int row)
    {
        if (nullptr == g_state.pTrackCache || 0 > row || row >= static_cast<int>(g_state.pTrackCache->size()))
        {
            return;
        }
        const double vol = (*g_state.pTrackCache)[row].Volume;
        wchar_t buf[SmallTextBufferChars] = { 0 };
        wsprintfW(buf, L"%d%%", static_cast<int>((vol * PercentScale) + VolumePercentRoundingBias));
        ListView_SetItemText(g_hTrackList, row, ColIndexVolume, buf);
    }

    void PopulateTrackList()
    {
        ListView_DeleteAllItems(g_hTrackList);
        if (nullptr == g_state.pMidiFile)
        {
            return;
        }

        auto pSummaries = MidiParser::GetTrackSummaries(*g_state.pMidiFile);

        int row = 0;
        for (const auto& info : *pSummaries)
        {
            LVITEMW item = { 0 };
            item.mask = LVIF_TEXT;
            item.iItem = row;

            wchar_t indexBuf[SmallTextBufferChars] = { 0 };
            wsprintfW(indexBuf, L"%u", info.TrackIndex);
            item.iSubItem = ColIndexTrackNumber;
            item.pszText = indexBuf;
            ListView_InsertItem(g_hTrackList, &item);

            wchar_t countBuf[SmallTextBufferChars] = { 0 };
            wsprintfW(countBuf, L"%u", info.NoteCount);
            ListView_SetItemText(g_hTrackList, row, ColIndexNoteCount, countBuf);

            auto pInstr = Utf8ToWide(info.InstrumentName);
            ListView_SetItemText(g_hTrackList, row, ColIndexInstrument, pInstr->empty() ? const_cast<LPWSTR>(L"-") : &(*pInstr)[0]);

            UpdateVolumeCell(row);

            ListView_SetCheckState(g_hTrackList, row, TRUE);
            ++row;
        }
    }

    void BuildTrackCache()
    {
        g_state.pTrackCache = std::make_unique<std::vector<TrackCacheEntry>>();
        g_state.TotalDurationMs = 0;
        g_state.ZoomX = MinZoomX;
        g_state.ScrollXMs = 0;

        if (nullptr == g_state.pMidiFile)
        {
            return;
        }

        g_state.pTempoMap = NoteConverter::BuildTempoMap(*g_state.pMidiFile);

        uint32_t idx = 0;
        for (const auto& track : *g_state.pMidiFile->pTracks)
        {
            TrackCacheEntry entry;
            entry.IsSelected = true;
            entry.ColorRef = static_cast<unsigned int>(TrackPalette[idx % TrackPaletteSize]);
            entry.LowestNote = InitialLowestNote;
            entry.HighestNote = InitialHighestNote;
            entry.Volume = DefaultVolume;
            entry.pNotes = NoteConverter::ConvertNotes(*track.pNotes, *g_state.pTempoMap, g_state.pMidiFile->TicksPerBeat, DefaultVelocityScale);

            for (const auto& note : *entry.pNotes)
            {
                const unsigned int endMs = note.StartMs + note.DurationMs;
                if (endMs > g_state.TotalDurationMs)
                {
                    g_state.TotalDurationMs = endMs;
                }
            }

            for (const auto& midi : *track.pNotes)
            {
                if (midi.NoteNumber < entry.LowestNote)
                {
                    entry.LowestNote = midi.NoteNumber;
                }
                if (midi.NoteNumber > entry.HighestNote)
                {
                    entry.HighestNote = midi.NoteNumber;
                }
            }

            g_state.pTrackCache->push_back(std::move(entry));
            ++idx;
        }

        RecomputeSelectedPitchRange();
    }

    unsigned int VisibleWindowMs()
    {
        if (0 == g_state.TotalDurationMs)
        {
            return 0;
        }
        double zoom = g_state.ZoomX;
        if (MinZoomX > zoom)
        {
            zoom = MinZoomX;
        }
        const double visible = static_cast<double>(g_state.TotalDurationMs) / zoom;

        return static_cast<unsigned int>(visible);
    }

    void ClampScroll()
    {
        const unsigned int visible = VisibleWindowMs();
        if (visible >= g_state.TotalDurationMs)
        {
            g_state.ScrollXMs = 0;
            return;
        }
        const unsigned int maxScroll = g_state.TotalDurationMs - visible;
        if (g_state.ScrollXMs > maxScroll)
        {
            g_state.ScrollXMs = maxScroll;
        }
    }

    void UpdateScrollInfo()
    {
        if (nullptr == g_hPianoRoll)
        {
            return;
        }

        ClampScroll();

        SCROLLINFO si = { 0 };
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
        si.nMin = 0;

        if (0 == g_state.TotalDurationMs)
        {
            si.nMax = 0;
            si.nPage = 1;
            si.nPos = 0;
        }
        else
        {
            si.nMax = static_cast<int>(g_state.TotalDurationMs);
            si.nPage = VisibleWindowMs();
            si.nPos = static_cast<int>(g_state.ScrollXMs);
        }

        SetScrollInfo(g_hPianoRoll, SB_HORZ, &si, TRUE);
    }

    unsigned int ClientXToMs(int clientX)
    {
        RECT rc;
        GetClientRect(g_hPianoRoll, &rc);
        const int clientWidth = rc.right - rc.left;
        if (0 >= clientWidth || 0 == g_state.TotalDurationMs)
        {
            return 0;
        }
        double zoom = g_state.ZoomX;
        if (MinZoomX > zoom)
        {
            zoom = MinZoomX;
        }
        const double ppm = (static_cast<double>(clientWidth) / static_cast<double>(g_state.TotalDurationMs)) * zoom;
        if (0.0 >= ppm)
        {
            return 0;
        }
        const double ms = static_cast<double>(g_state.ScrollXMs) + (static_cast<double>(clientX) / ppm);

        return static_cast<unsigned int>(ms);
    }

    void ApplyZoomAtAnchor(double newZoom, unsigned int anchorMs)
    {
        if (MinZoomX > newZoom)
        {
            newZoom = MinZoomX;
        }
        if (MaxZoomX < newZoom)
        {
            newZoom = MaxZoomX;
        }

        RECT rc;
        GetClientRect(g_hPianoRoll, &rc);
        const int clientWidth = rc.right - rc.left;
        if (0 >= clientWidth || 0 == g_state.TotalDurationMs)
        {
            g_state.ZoomX = newZoom;
            UpdateScrollInfo();
            InvalidateRect(g_hPianoRoll, nullptr, TRUE);
            return;
        }

        const double oldPpm = (static_cast<double>(clientWidth) / static_cast<double>(g_state.TotalDurationMs)) * g_state.ZoomX;
        const double anchorPx = (static_cast<double>(anchorMs) - static_cast<double>(g_state.ScrollXMs)) * oldPpm;

        g_state.ZoomX = newZoom;

        const double newPpm = (static_cast<double>(clientWidth) / static_cast<double>(g_state.TotalDurationMs)) * g_state.ZoomX;
        const double newScrollMs = static_cast<double>(anchorMs) - (anchorPx / newPpm);

        if (0.0 > newScrollMs)
        {
            g_state.ScrollXMs = 0;
        }
        else
        {
            g_state.ScrollXMs = static_cast<unsigned int>(newScrollMs);
        }

        UpdateScrollInfo();
        InvalidateRect(g_hPianoRoll, nullptr, TRUE);
    }

    void ScrollByMs(int deltaMs)
    {
        const long long newVal = static_cast<long long>(g_state.ScrollXMs) + deltaMs;
        if (0 > newVal)
        {
            g_state.ScrollXMs = 0;
        }
        else
        {
            g_state.ScrollXMs = static_cast<unsigned int>(newVal);
        }
        UpdateScrollInfo();
        InvalidateRect(g_hPianoRoll, nullptr, TRUE);
    }

    void HandleHScroll(WPARAM wParam)
    {
        const WORD code = LOWORD(wParam);
        const unsigned int visible = VisibleWindowMs();
        const int lineStep = static_cast<int>(visible / ScrollLineStepDivisor) + 1;
        const int pageStep = static_cast<int>(visible);

        SCROLLINFO si = { 0 };
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(g_hPianoRoll, SB_HORZ, &si);

        int newPos = si.nPos;
        switch (code)
        {
        case SB_LINELEFT:
        {
            newPos -= lineStep;
            break;
        }
        case SB_LINERIGHT:
        {
            newPos += lineStep;
            break;
        }
        case SB_PAGELEFT:
        {
            newPos -= pageStep;
            break;
        }
        case SB_PAGERIGHT:
        {
            newPos += pageStep;
            break;
        }
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
        {
            newPos = si.nTrackPos;
            break;
        }
        case SB_LEFT:
        {
            newPos = 0;
            break;
        }
        case SB_RIGHT:
        {
            newPos = si.nMax;
            break;
        }
        default:
        {
            break;
        }
        }

        if (0 > newPos)
        {
            newPos = 0;
        }
        g_state.ScrollXMs = static_cast<unsigned int>(newPos);
        UpdateScrollInfo();
        InvalidateRect(g_hPianoRoll, nullptr, TRUE);
    }

    void HandleMouseWheel(WPARAM wParam, LPARAM lParam)
    {
        const short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        const WORD keys = GET_KEYSTATE_WPARAM(wParam);
        const int ticks = zDelta / WheelDeltaStd;

        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        ScreenToClient(g_hPianoRoll, &pt);

        if (0 != (keys & MK_CONTROL))
        {
            const unsigned int anchor = ClientXToMs(pt.x);
            double zoom = g_state.ZoomX;
            if (0 < ticks)
            {
                for (int i = 0; i < ticks; ++i)
                {
                    zoom *= ZoomStep;
                }
            }
            else
            {
                for (int i = 0; i < -ticks; ++i)
                {
                    zoom /= ZoomStep;
                }
            }
            ApplyZoomAtAnchor(zoom, anchor);
        }
        else
        {
            const int visible = static_cast<int>(VisibleWindowMs());
            const int step = (visible / WheelScrollDivisor) + 1;
            ScrollByMs(-ticks * step);
        }
    }

    void LoadMidiFile(const std::wstring& widePath)
    {
        try
        {
            const std::string utf8Path = WideToUtf8(widePath.c_str());
            ClearAppState();

            g_state.pCurrentFilePath = std::make_unique<std::string>(utf8Path);
            g_state.pMidiFile = MidiParser::Parse(utf8Path);
            BuildTrackCache();
            PopulateTrackList();
            UpdateScrollInfo();

            wchar_t status[StatusTextMax] = { 0 };
            wsprintfW(status, L"Loaded: %s  (%u tracks, %u ms)", widePath.c_str(), static_cast<unsigned int>(g_state.pMidiFile->pTracks->size()), g_state.TotalDurationMs);
            SetStatusText(status);

            InvalidateRect(g_hPianoRoll, nullptr, TRUE);
        }
        catch (const std::exception& ex)
        {
            auto pMsg = Utf8ToWide(std::string("Failed to load MIDI:\n") + ex.what());
            MessageBoxW(g_hMain, pMsg->c_str(), L"MidiViewer", MB_OK | MB_ICONERROR);
            ClearAppState();
            ListView_DeleteAllItems(g_hTrackList);
            InvalidateRect(g_hPianoRoll, nullptr, TRUE);
        }
    }

    void HandleOpenCommand()
    {
        auto pPath = std::make_unique<std::vector<wchar_t>>(MaxFilePathChars, L'\0');

        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hMain;
        ofn.lpstrFilter = L"MIDI Files (*.mid;*.midi)\0*.mid;*.midi\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = pPath->data();
        ofn.nMaxFile = MaxFilePathChars;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
        ofn.lpstrTitle = L"Open MIDI File";

        if (TRUE == GetOpenFileNameW(&ofn))
        {
            LoadMidiFile(std::wstring(pPath->data()));
        }
    }

    std::unique_ptr<std::vector<ConvertedNote>> CollectCheckedNotes()
    {
        return NoteFilters::BuildMergedNotes(&g_state, true);
    }

    int CountCheckedTracksWithNotes()
    {
        if (nullptr == g_state.pTrackCache)
        {
            return 0;
        }
        int count = 0;
        for (const auto& entry : *g_state.pTrackCache)
        {
            if (true == entry.IsSelected && nullptr != entry.pNotes && false == entry.pNotes->empty())
            {
                ++count;
            }
        }

        return count;
    }

    void HandlePlayCommand()
    {
        if (nullptr == g_state.pTrackCache || true == g_state.pTrackCache->empty())
        {
            return;
        }

        auto pMerged = CollectCheckedNotes();
        if (true == pMerged->empty())
        {
            SetStatusText(L"No checked track has any notes.");
            return;
        }

        const int trackCount = CountCheckedTracksWithNotes();

        TrackPlayer::Stop();
        KillTimer(g_hMain, PlaybackTimerId);
        g_state.PlaybackCursorMs = NoPlaybackCursor;

        const size_t noteCount = pMerged->size();
        if (true == TrackPlayer::Start(std::move(pMerged)))
        {
            g_state.PlaybackCursorMs = 0;
            SetTimer(g_hMain, PlaybackTimerId, PlaybackTimerIntervalMs, nullptr);
            InvalidateRect(g_hPianoRoll, nullptr, FALSE);

            wchar_t status[StatusTextMax] = { 0 };
            wsprintfW(status, L"Playing %d tracks (%zu notes)...", trackCount, noteCount);
            SetStatusText(status);
        }
    }

    void HandleStopCommand()
    {
        TrackPlayer::Stop();
        KillTimer(g_hMain, PlaybackTimerId);
        g_state.PlaybackCursorMs = NoPlaybackCursor;
        InvalidateRect(g_hPianoRoll, nullptr, FALSE);
        SetStatusText(L"Stopped.");
    }

    void HandlePlaybackTick()
    {
        if (false == TrackPlayer::IsPlaying())
        {
            KillTimer(g_hMain, PlaybackTimerId);
            g_state.PlaybackCursorMs = NoPlaybackCursor;
            InvalidateRect(g_hPianoRoll, nullptr, FALSE);
            SetStatusText(L"Playback finished.");
            return;
        }

        g_state.PlaybackCursorMs = TrackPlayer::GetElapsedMs();
        InvalidateRect(g_hPianoRoll, nullptr, FALSE);
    }

    void AnnounceFilterChange()
    {
        UpdateFilterMenuState();
        InvalidateRect(g_hPianoRoll, nullptr, FALSE);

        wchar_t status[StatusTextMax] = { 0 };
        wsprintfW(status, L"Filters: Mono=%s, Clamp=%s, Octave=%+d",
            g_state.FilterMonophonic ? L"on" : L"off",
            g_state.FilterClampAudible ? L"on" : L"off",
            g_state.OctaveShift);
        SetStatusText(status);
    }

    void HandleFilterToggleMono()
    {
        g_state.FilterMonophonic = !g_state.FilterMonophonic;
        AnnounceFilterChange();
    }

    void HandleFilterToggleClamp()
    {
        g_state.FilterClampAudible = !g_state.FilterClampAudible;
        AnnounceFilterChange();
    }

    void HandleFilterOctaveShift(int delta)
    {
        int next = g_state.OctaveShift + delta;
        if (MinOctave > next)
        {
            next = MinOctave;
        }
        if (MaxOctave < next)
        {
            next = MaxOctave;
        }
        g_state.OctaveShift = next;
        AnnounceFilterChange();
    }

    void HandleFilterOctaveReset()
    {
        g_state.OctaveShift = 0;
        AnnounceFilterChange();
    }

    void HandleInstallCommand()
    {
        if (nullptr == g_state.pTrackCache || true == g_state.pTrackCache->empty())
        {
            SetStatusText(L"Load a MIDI file first.");
            return;
        }

        ExportDialog::ExportInfo info;
        info.BpmEstimate = 0;
        if (false == ExportDialog::Show(g_hMain, &info))
        {
            return;
        }

        auto pResult = BeepSongsInstaller::Install(&g_state, info);
        auto pWide = Utf8ToWide(*pResult);

        const bool ok = (0 == pResult->rfind("OK", 0));
        MessageBoxW(g_hMain, pWide->c_str(), L"Install to BeepSongs", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));

        if (true == ok)
        {
            SetStatusText(L"Installed to BeepSongs.");
        }
        else
        {
            SetStatusText(L"Install failed - see message box.");
        }
    }

    void AdjustTrackVolume(int row, double delta)
    {
        if (nullptr == g_state.pTrackCache || 0 > row || row >= static_cast<int>(g_state.pTrackCache->size()))
        {
            return;
        }
        TrackCacheEntry& entry = (*g_state.pTrackCache)[row];
        double next = entry.Volume + delta;
        if (MinVolume > next)
        {
            next = MinVolume;
        }
        if (MaxVolume < next)
        {
            next = MaxVolume;
        }
        entry.Volume = next;
        UpdateVolumeCell(row);
    }

    int HitTestVolumeColumn(LPARAM lParam)
    {
        const NMITEMACTIVATE* pAct = reinterpret_cast<const NMITEMACTIVATE*>(lParam);
        if (0 > pAct->iItem)
        {
            return -1;
        }

        LVHITTESTINFO hti = { 0 };
        hti.pt = pAct->ptAction;
        ListView_SubItemHitTest(g_hTrackList, &hti);
        if (ColIndexVolume != hti.iSubItem)
        {
            return -1;
        }

        return hti.iItem;
    }

    void OnTrackListNotify(LPARAM lParam)
    {
        const NMHDR* pHdr = reinterpret_cast<const NMHDR*>(lParam);
        if (NM_CLICK == pHdr->code)
        {
            const int row = HitTestVolumeColumn(lParam);
            if (0 <= row)
            {
                AdjustTrackVolume(row, VolumeStep);
            }
            return;
        }
        if (NM_RCLICK == pHdr->code)
        {
            const int row = HitTestVolumeColumn(lParam);
            if (0 <= row)
            {
                AdjustTrackVolume(row, -VolumeStep);
            }
            return;
        }
        if (LVN_ITEMCHANGED != pHdr->code)
        {
            return;
        }
        const NMLISTVIEW* pLv = reinterpret_cast<const NMLISTVIEW*>(lParam);
        if (0 == (pLv->uChanged & LVIF_STATE))
        {
            return;
        }
        const UINT oldImg = (pLv->uOldState & LVIS_STATEIMAGEMASK) >> StateImageShift;
        const UINT newImg = (pLv->uNewState & LVIS_STATEIMAGEMASK) >> StateImageShift;
        if (oldImg == newImg || 0 == oldImg)
        {
            return;
        }
        if (nullptr == g_state.pTrackCache || 0 > pLv->iItem || pLv->iItem >= static_cast<int>(g_state.pTrackCache->size()))
        {
            return;
        }

        (*g_state.pTrackCache)[pLv->iItem].IsSelected = (CheckedStateImage == newImg);
        RecomputeSelectedPitchRange();
        UpdateScrollInfo();
        InvalidateRect(g_hPianoRoll, nullptr, TRUE);
    }

    LRESULT CALLBACK PianoRollProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
        {
            return 1;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            const HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            const int width = rc.right - rc.left;
            const int height = rc.bottom - rc.top;

            const HDC hMemDC = CreateCompatibleDC(hdc);
            const HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, width, height);
            const HGDIOBJ hOldBmp = SelectObject(hMemDC, hMemBmp);

            TrackRenderer::Paint(hMemDC, rc, &g_state);
            BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);

            SelectObject(hMemDC, hOldBmp);
            DeleteObject(hMemBmp);
            DeleteDC(hMemDC);

            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_SIZE:
        {
            UpdateScrollInfo();
            return 0;
        }
        case WM_HSCROLL:
        {
            HandleHScroll(wParam);
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            HandleMouseWheel(wParam, lParam);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        {
            SetFocus(hWnd);
            return 0;
        }
        default:
        {
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
        }
    }

    void HandleCommand(WPARAM wParam)
    {
        const WORD id = LOWORD(wParam);
        switch (id)
        {
        case IDM_FILE_OPEN:
        {
            HandleOpenCommand();
            break;
        }
        case IDM_FILE_EXIT:
        {
            DestroyWindow(g_hMain);
            break;
        }
        case IDM_EXPORT_INSTALL:
        {
            HandleInstallCommand();
            break;
        }
        case IDM_PLAY_SELECTED:
        case IDC_PLAY_BUTTON:
        {
            HandlePlayCommand();
            break;
        }
        case IDM_PLAY_STOP:
        case IDC_STOP_BUTTON:
        {
            HandleStopCommand();
            break;
        }
        case IDM_FILTER_MONOPHONIC:
        {
            HandleFilterToggleMono();
            break;
        }
        case IDM_FILTER_CLAMP:
        {
            HandleFilterToggleClamp();
            break;
        }
        case IDM_FILTER_OCTAVE_UP:
        {
            HandleFilterOctaveShift(1);
            break;
        }
        case IDM_FILTER_OCTAVE_DOWN:
        {
            HandleFilterOctaveShift(-1);
            break;
        }
        case IDM_FILTER_OCTAVE_RESET:
        {
            HandleFilterOctaveReset();
            break;
        }
        case IDM_HELP_ABOUT:
        {
            MessageBoxW(g_hMain, L"MidiViewer\nVisualize MIDI tracks, audition each one, and install them into BeepSongs as .beep song files.", L"About MidiViewer", MB_OK | MB_ICONINFORMATION);
            break;
        }
        default:
        {
            break;
        }
        }
    }

    LRESULT CALLBACK MainProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_CREATE:
        {
            g_hMain = hWnd;
            CreateChildren();
            return 0;
        }
        case WM_SIZE:
        {
            LayoutChildren(LOWORD(lParam), HIWORD(lParam));
            return 0;
        }
        case WM_COMMAND:
        {
            HandleCommand(wParam);
            return 0;
        }
        case WM_TIMER:
        {
            if (PlaybackTimerId == wParam)
            {
                HandlePlaybackTick();
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);

            RECT rollScreen;
            GetWindowRect(g_hPianoRoll, &rollScreen);
            if (TRUE == PtInRect(&rollScreen, pt))
            {
                return SendMessageW(g_hPianoRoll, WM_MOUSEWHEEL, wParam, lParam);
            }
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
        case WM_NOTIFY:
        {
            const NMHDR* pHdr = reinterpret_cast<const NMHDR*>(lParam);
            if (static_cast<int>(IDC_TRACK_LIST) == static_cast<int>(pHdr->idFrom))
            {
                OnTrackListNotify(lParam);
            }
            return 0;
        }
        case WM_CLOSE:
        {
            TrackPlayer::Stop();
            DestroyWindow(hWnd);
            return 0;
        }
        case WM_DESTROY:
        {
            TrackPlayer::Stop();
            ClearAppState();
            PostQuitMessage(0);
            return 0;
        }
        default:
        {
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
        }
    }
}

namespace MainWindow
{
    bool Register(HINSTANCE hInstance)
    {
        g_hInstance = hInstance;
        g_state.TotalDurationMs = 0;
        g_state.LowestNote = InitialLowestNote;
        g_state.HighestNote = InitialHighestNote;
        g_state.ZoomX = MinZoomX;
        g_state.ScrollXMs = 0;
        g_state.PlaybackCursorMs = NoPlaybackCursor;
        g_state.FilterMonophonic = false;
        g_state.FilterClampAudible = false;
        g_state.OctaveShift = 0;

        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = MainProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = MainClassName;
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
        if (0 == RegisterClassExW(&wc))
        {
            return false;
        }

        WNDCLASSEXW pc = { 0 };
        pc.cbSize = sizeof(pc);
        pc.style = CS_HREDRAW | CS_VREDRAW;
        pc.lpfnWndProc = PianoRollProc;
        pc.hInstance = hInstance;
        pc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        pc.hbrBackground = nullptr;
        pc.lpszClassName = PianoRollClassName;
        if (0 == RegisterClassExW(&pc))
        {
            return false;
        }

        return true;
    }

    HWND Create(HINSTANCE hInstance, int showCmd)
    {
        const HMENU hMenu = BuildMainMenu();

        const HWND hWnd = CreateWindowExW(0, MainClassName, WindowTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, InitialWidth, InitialHeight,
            nullptr, hMenu, hInstance, nullptr);
        if (nullptr == hWnd)
        {
            return nullptr;
        }

        ShowWindow(hWnd, showCmd);
        UpdateWindow(hWnd);
        return hWnd;
    }

    int RunMessageLoop()
    {
        MSG msg;
        while (0 < GetMessageW(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }
}
