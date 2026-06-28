#include <Windows.h>
#include <array>
#include <iostream>
#include <memory>
#include <string>

#include "AudioEngine.h"
#include "CommandParser.h"
#include "Pattern.h"
#include "Renderer.h"

namespace
{
    constexpr double DefaultVolume = 0.8;
    constexpr int MaxBindSlots = 10;
    constexpr int PollTimeoutMs = 50;
    constexpr WORD CtrlKeyState = LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED;

    void ExecutePlayOnce(AudioEngine& engine, const Pattern& pattern, const ParsedCommand& cmd)
    {
        OneShotSound sound = {};
        sound.Type = cmd.Instrument;
        sound.Waveform = cmd.Waveform;
        sound.Drum = cmd.Drum;
        sound.FrequencyHz = cmd.FrequencyHz;
        sound.Volume = DefaultVolume;
        sound.Modulation = cmd.Modulation;

        if (InstrumentType::Synth == cmd.Instrument)
        {
            sound.DurationSamples = pattern.GetStepDurationSamples() * static_cast<DWORD>(cmd.DurationSteps);
        }
        else
        {
            sound.DurationSamples = Synthesizer::GetDrumDurationSamples(cmd.Drum);
        }

        engine.QueueSound(sound);

        if (cmd.DurationSteps > 1)
        {
            std::cout << "  -> " << cmd.InstrumentKey << " (" << cmd.DurationSteps << " steps)" << std::endl;
        }
        else
        {
            std::cout << "  -> " << cmd.InstrumentKey << std::endl;
        }
    }

    void ExecuteLoop(Pattern& pattern, const ParsedCommand& cmd)
    {
        pattern.SetLoop(cmd.InstrumentKey, cmd.Instrument, cmd.Waveform, cmd.Drum, cmd.FrequencyHz, cmd.Modulation, *cmd.pSteps);
        std::cout << "  -> looping " << cmd.InstrumentKey << " on " << cmd.pSteps->size() << " steps" << std::endl;
    }

    void ExecuteCommand(const std::string& input, AudioEngine& engine, Pattern& pattern, std::array<std::string, MaxBindSlots>& binds, bool& running)
    {
        ParsedCommand cmd = CommandParser::Parse(input);

        switch (cmd.Type)
        {
        case CommandType::PlayOnce:
            ExecutePlayOnce(engine, pattern, cmd);
            break;
        case CommandType::Loop:
            ExecuteLoop(pattern, cmd);
            Renderer::PrintStatus(pattern);
            break;
        case CommandType::Stop:
            pattern.ClearLoop(cmd.InstrumentKey);
            std::cout << "  -> stopped " << cmd.InstrumentKey << std::endl;
            Renderer::PrintStatus(pattern);
            break;
        case CommandType::StopAll:
            pattern.ClearAllLoops();
            std::cout << "  -> all loops stopped" << std::endl;
            break;
        case CommandType::SetBpm:
            pattern.SetBpm(cmd.BpmValue);
            std::cout << "  -> BPM set to " << pattern.GetBpm() << std::endl;
            break;
        case CommandType::GetBpm:
            std::cout << "  BPM: " << pattern.GetBpm() << std::endl;
            break;
        case CommandType::Bind:
            if (cmd.BindSlot >= 0 && cmd.BindSlot < MaxBindSlots)
            {
                binds[static_cast<size_t>(cmd.BindSlot)] = cmd.BindCommand;
                std::cout << "  -> Ctrl+" << cmd.BindSlot << " = " << cmd.BindCommand << std::endl;
            }
            break;
        case CommandType::Binds:
            std::cout << "  Binds:" << std::endl;
            for (int i = 0; i < MaxBindSlots; ++i)
            {
                if (!binds[static_cast<size_t>(i)].empty())
                {
                    std::cout << "    Ctrl+" << i << " = " << binds[static_cast<size_t>(i)] << std::endl;
                }
            }
            break;
        case CommandType::Help:
            Renderer::PrintHelp();
            break;
        case CommandType::Clear:
            system("cls");
            Renderer::PrintBanner();
            Renderer::PrintStatus(pattern);
            break;
        case CommandType::Exit:
            running = false;
            std::cout << "  Goodbye!" << std::endl;
            break;
        case CommandType::Invalid:
            if (!cmd.ErrorMessage.empty()) { std::cout << "  Error: " << cmd.ErrorMessage << std::endl; }
            break;
        }
    }
}

int main()
{
    auto pPattern = std::make_unique<Pattern>();
    auto pEngine = std::make_unique<AudioEngine>();
    auto pBinds = std::make_unique<std::array<std::string, MaxBindSlots>>();

    HANDLE consoleIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE consoleOut = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD originalInMode = 0;
    GetConsoleMode(consoleIn, &originalInMode);
    SetConsoleMode(consoleIn, ENABLE_WINDOW_INPUT);

    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(consoleOut, &cursorInfo);

    Renderer::PrintBanner();

    if (!pEngine->Start(pPattern.get()))
    {
        std::cerr << "  Error: Could not open audio device." << std::endl;
        SetConsoleMode(consoleIn, originalInMode);
        return 1;
    }

    bool running = true;
    auto pInputBuffer = std::make_unique<std::string>();
    std::cout << "beep> ";

    while (running)
    {
        DWORD eventCount = 0;
        GetNumberOfConsoleInputEvents(consoleIn, &eventCount);

        if (0 == eventCount)
        {
            Sleep(static_cast<DWORD>(PollTimeoutMs));
            continue;
        }

        INPUT_RECORD record = {};
        DWORD eventsRead = 0;
        ReadConsoleInput(consoleIn, &record, 1, &eventsRead);

        if (0 == eventsRead || KEY_EVENT != record.EventType || !record.Event.KeyEvent.bKeyDown) { continue; }

        const WORD vk = record.Event.KeyEvent.wVirtualKeyCode;
        const char ascii = record.Event.KeyEvent.uChar.AsciiChar;
        const bool ctrlHeld = 0 != (record.Event.KeyEvent.dwControlKeyState & CtrlKeyState);

        // Ctrl+0 through Ctrl+9 triggers bound command
        if (ctrlHeld && vk >= '0' && vk <= '9')
        {
            const int slot = vk - '0';
            if (!(*pBinds)[static_cast<size_t>(slot)].empty())
            {
                std::cout << std::endl << "  [Ctrl+" << slot << "] ";
                ExecuteCommand((*pBinds)[static_cast<size_t>(slot)], *pEngine, *pPattern, *pBinds, running);
                std::cout << "beep> " << *pInputBuffer;
            }
            continue;
        }

        // Enter: execute the typed command
        if (VK_RETURN == vk)
        {
            std::cout << std::endl;
            if (!pInputBuffer->empty())
            {
                ExecuteCommand(*pInputBuffer, *pEngine, *pPattern, *pBinds, running);
                pInputBuffer->clear();
            }
            if (running) { std::cout << "beep> "; }
            continue;
        }

        // Backspace
        if (VK_BACK == vk && !pInputBuffer->empty())
        {
            pInputBuffer->pop_back();
            std::cout << "\b \b";
            continue;
        }

        // Printable character
        if (ascii >= ' ' && ascii <= '~')
        {
            pInputBuffer->push_back(ascii);
            std::cout << ascii;
        }
    }

    pEngine->Stop();
    SetConsoleMode(consoleIn, originalInMode);
    return 0;
}
