#include <Windows.h>
#include <commctrl.h>

#include "MainWindow.h"

namespace
{
    constexpr DWORD CommonControlsMask = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), CommonControlsMask };
    InitCommonControlsEx(&icex);

    if (false == MainWindow::Register(hInstance))
    {
        return 1;
    }

    const HWND hWnd = MainWindow::Create(hInstance, nShowCmd);
    if (nullptr == hWnd)
    {
        return 1;
    }

    return MainWindow::RunMessageLoop();
}
