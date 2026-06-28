#pragma once

#include <Windows.h>

/// @file InputHandler.h
/// @brief Provides non-blocking keyboard input polling for the
///        TUI beat maker using Windows Console API.

enum class Action
{
    None,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    ToggleStep,
    IncreaseBpm,
    DecreaseBpm,
    Quit
};

class InputHandler
{
public:
    InputHandler();

    /// @brief Sets up the console for non-blocking input.
    void Initialize();

    /// @brief Restores original console input mode.
    void Restore();

    /// @brief Polls for a single keyboard action without blocking.
    /// @return The action corresponding to the key pressed, or None.
    Action Poll();

private:
    HANDLE ConsoleIn;
    DWORD OriginalMode;
};
