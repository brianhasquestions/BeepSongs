#include "InputHandler.h"

InputHandler::InputHandler() : ConsoleIn(nullptr), OriginalMode(0)
{
}

void InputHandler::Initialize()
{
    ConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(ConsoleIn, &OriginalMode);

    const DWORD inputMode = ENABLE_WINDOW_INPUT;
    SetConsoleMode(ConsoleIn, inputMode);
}

void InputHandler::Restore()
{
    if (nullptr != ConsoleIn)
    {
        SetConsoleMode(ConsoleIn, OriginalMode);
    }
}

Action InputHandler::Poll()
{
    DWORD eventCount = 0;
    GetNumberOfConsoleInputEvents(ConsoleIn, &eventCount);

    if (0 == eventCount)
    {
        return Action::None;
    }

    INPUT_RECORD record = {};
    DWORD eventsRead = 0;
    ReadConsoleInput(ConsoleIn, &record, 1, &eventsRead);

    if (0 == eventsRead)
    {
        return Action::None;
    }

    if (KEY_EVENT != record.EventType)
    {
        return Action::None;
    }

    if (!record.Event.KeyEvent.bKeyDown)
    {
        return Action::None;
    }

    const WORD vk = record.Event.KeyEvent.wVirtualKeyCode;
    const char ascii = record.Event.KeyEvent.uChar.AsciiChar;

    if (VK_UP == vk)
    {
        return Action::MoveUp;
    }
    if (VK_DOWN == vk)
    {
        return Action::MoveDown;
    }
    if (VK_LEFT == vk)
    {
        return Action::MoveLeft;
    }
    if (VK_RIGHT == vk)
    {
        return Action::MoveRight;
    }
    if (VK_SPACE == vk)
    {
        return Action::ToggleStep;
    }
    if ('+' == ascii || VK_OEM_PLUS == vk)
    {
        return Action::IncreaseBpm;
    }
    if ('-' == ascii || VK_OEM_MINUS == vk)
    {
        return Action::DecreaseBpm;
    }
    if ('q' == ascii || 'Q' == ascii)
    {
        return Action::Quit;
    }

    return Action::None;
}
