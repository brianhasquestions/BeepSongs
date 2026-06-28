#pragma once

#include <Windows.h>

/// @file MainWindow.h
/// @brief Declares entry points for creating and running the
///        MidiViewer top-level window and its child controls.

namespace MainWindow
{
    /// @brief Registers the main window class with the system.
    /// @param hInstance The application instance handle.
    /// @return True on success, false on failure.
    bool Register(HINSTANCE hInstance);

    /// @brief Creates the main window and all child controls.
    /// @param hInstance The application instance handle.
    /// @param showCmd The initial show-state from WinMain.
    /// @return Handle to the created main window, or null on failure.
    HWND Create(HINSTANCE hInstance, int showCmd);

    /// @brief Runs the standard GetMessage / DispatchMessage loop
    ///        until the window is closed.
    /// @return The WM_QUIT exit code to return from WinMain.
    int RunMessageLoop();
}
