#include "ExportDialog.h"

#include <Windows.h>
#include <memory>
#include <string>

namespace
{
    constexpr wchar_t DialogClassName[] = L"MidiViewerExportDialog";
    constexpr wchar_t DialogTitle[] = L"Install to BeepSongs";

    constexpr int DialogWidth = 520;
    constexpr int DialogHeight = 260;
    constexpr int FieldLabelX = 16;
    constexpr int FieldX = 150;
    constexpr int FieldWidth = 340;
    constexpr int FieldHeight = 22;
    constexpr int RowSpacing = 32;
    constexpr int FirstRowY = 16;
    constexpr int ButtonY = 184;
    constexpr int ButtonWidth = 100;
    constexpr int ButtonHeight = 28;
    constexpr int ButtonSpacing = 12;
    constexpr int DialogButtonCount = 2;
    constexpr int LabelPadInset = 4;
    constexpr int EditBufferMax = 256;
    constexpr uint32_t DefaultBpm = 120;

    constexpr int IdBaseName = 1001;
    constexpr int IdDisplayName = 1002;
    constexpr int IdDescription = 1003;
    constexpr int IdBpm = 1004;
    constexpr int IdOk = 1010;
    constexpr int IdCancel = 1011;

    struct DialogState
    {
        HWND hEditBase;
        HWND hEditDisplay;
        HWND hEditDesc;
        HWND hEditBpm;
        bool accepted;
        ExportDialog::ExportInfo* pOut;
    };

    std::string WideEditToUtf8(HWND hEdit)
    {
        wchar_t buf[EditBufferMax] = { 0 };
        GetWindowTextW(hEdit, buf, EditBufferMax);
        const int needed = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
        if (1 >= needed)
        {
            return std::string();
        }
        std::string out(static_cast<size_t>(needed - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, &out[0], needed, nullptr, nullptr);

        return out;
    }

    void CreateRow(HWND hParent, int rowIndex, const wchar_t* pLabel, HWND* pOutEdit, int editId)
    {
        const int y = FirstRowY + (rowIndex * RowSpacing);
        CreateWindowExW(0, L"STATIC", pLabel,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            FieldLabelX, y + LabelPadInset, FieldX - FieldLabelX - LabelPadInset, FieldHeight,
            hParent, nullptr, nullptr, nullptr);

        *pOutEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            FieldX, y, FieldWidth, FieldHeight,
            hParent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(editId)), nullptr, nullptr);
    }

    void CreateChildren(HWND hWnd, DialogState* pState)
    {
        CreateRow(hWnd, 0, L"Base name:", &pState->hEditBase, IdBaseName);
        CreateRow(hWnd, 1, L"Display name:", &pState->hEditDisplay, IdDisplayName);
        CreateRow(hWnd, 2, L"Description:", &pState->hEditDesc, IdDescription);
        CreateRow(hWnd, 3, L"BPM:", &pState->hEditBpm, IdBpm);

        SetWindowTextW(pState->hEditBpm, std::to_wstring(DefaultBpm).c_str());

        const int totalButtonsWidth = (ButtonWidth * DialogButtonCount) + ButtonSpacing;
        const int buttonsX = (DialogWidth - totalButtonsWidth) / 2;

        CreateWindowExW(0, L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            buttonsX, ButtonY, ButtonWidth, ButtonHeight,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdOk)), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            buttonsX + ButtonWidth + ButtonSpacing, ButtonY, ButtonWidth, ButtonHeight,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdCancel)), nullptr, nullptr);

        SetFocus(pState->hEditBase);
    }

    bool ValidateAndCommit(DialogState* pState)
    {
        const std::string baseName = WideEditToUtf8(pState->hEditBase);
        if (true == baseName.empty())
        {
            MessageBoxW(GetParent(pState->hEditBase), L"Base name is required.", L"Export", MB_OK | MB_ICONWARNING);
            SetFocus(pState->hEditBase);
            return false;
        }

        for (const char c : baseName)
        {
            const bool isAlpha = ('A' <= c && 'Z' >= c) || ('a' <= c && 'z' >= c);
            const bool isDigit = ('0' <= c && '9' >= c);
            const bool isUnder = ('_' == c);
            if (false == isAlpha && false == isDigit && false == isUnder)
            {
                MessageBoxW(GetParent(pState->hEditBase), L"Base name must be a valid C++ identifier (letters, digits, underscore).", L"Export", MB_OK | MB_ICONWARNING);
                SetFocus(pState->hEditBase);
                return false;
            }
        }
        if ('0' <= baseName[0] && '9' >= baseName[0])
        {
            MessageBoxW(GetParent(pState->hEditBase), L"Base name must not start with a digit.", L"Export", MB_OK | MB_ICONWARNING);
            SetFocus(pState->hEditBase);
            return false;
        }

        pState->pOut->BaseName = baseName;
        pState->pOut->DisplayName = WideEditToUtf8(pState->hEditDisplay);
        if (true == pState->pOut->DisplayName.empty())
        {
            pState->pOut->DisplayName = baseName;
        }
        pState->pOut->Description = WideEditToUtf8(pState->hEditDesc);

        wchar_t bpmBuf[EditBufferMax] = { 0 };
        GetWindowTextW(pState->hEditBpm, bpmBuf, EditBufferMax);
        int bpm = _wtoi(bpmBuf);
        if (0 >= bpm)
        {
            bpm = static_cast<int>(DefaultBpm);
        }
        pState->pOut->BpmEstimate = static_cast<uint32_t>(bpm);

        return true;
    }

    LRESULT CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        DialogState* pState = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CREATE:
        {
            const CREATESTRUCTW* pCs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            pState = reinterpret_cast<DialogState*>(pCs->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pState));
            CreateChildren(hWnd, pState);
            return 0;
        }
        case WM_COMMAND:
        {
            const WORD id = LOWORD(wParam);
            if (IdOk == id)
            {
                if (true == ValidateAndCommit(pState))
                {
                    pState->accepted = true;
                    DestroyWindow(hWnd);
                }
                return 0;
            }
            if (IdCancel == id)
            {
                pState->accepted = false;
                DestroyWindow(hWnd);
                return 0;
            }
            return 0;
        }
        case WM_CLOSE:
        {
            pState->accepted = false;
            DestroyWindow(hWnd);
            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        default:
        {
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
        }
    }

    bool EnsureClassRegistered()
    {
        WNDCLASSEXW wc = { 0 };
        if (0 != GetClassInfoExW(GetModuleHandleW(nullptr), DialogClassName, &wc))
        {
            return true;
        }

        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DialogProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = DialogClassName;

        return (0 != RegisterClassExW(&wc));
    }
}

namespace ExportDialog
{
    bool Show(HWND hOwner, ExportInfo* pOutInfo)
    {
        if (nullptr == pOutInfo)
        {
            return false;
        }
        if (false == EnsureClassRegistered())
        {
            return false;
        }

        auto pState = std::make_unique<DialogState>();
        pState->hEditBase = nullptr;
        pState->hEditDisplay = nullptr;
        pState->hEditDesc = nullptr;
        pState->hEditBpm = nullptr;
        pState->accepted = false;
        pState->pOut = pOutInfo;

        RECT ownerRect = { 0 };
        GetWindowRect(hOwner, &ownerRect);
        const int ownerW = ownerRect.right - ownerRect.left;
        const int ownerH = ownerRect.bottom - ownerRect.top;
        const int posX = ownerRect.left + ((ownerW - DialogWidth) / 2);
        const int posY = ownerRect.top + ((ownerH - DialogHeight) / 2);

        const HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, DialogClassName, DialogTitle,
            WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
            posX, posY, DialogWidth, DialogHeight,
            hOwner, nullptr, GetModuleHandleW(nullptr), pState.get());
        if (nullptr == hDlg)
        {
            return false;
        }

        EnableWindow(hOwner, FALSE);

        MSG msg;
        while (0 < GetMessageW(&msg, nullptr, 0, 0))
        {
            if (FALSE == IsDialogMessageW(hDlg, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        EnableWindow(hOwner, TRUE);
        SetForegroundWindow(hOwner);

        return pState->accepted;
    }
}
