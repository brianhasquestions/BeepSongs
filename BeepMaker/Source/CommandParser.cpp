#include "CommandParser.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    constexpr double A4Frequency = 440.0;
    constexpr int A4NoteNumber = 69;
    constexpr double SemitonesPerOctave = 12.0;

    constexpr int NoteC = 0;
    constexpr int NoteD = 2;
    constexpr int NoteE = 4;
    constexpr int NoteF = 5;
    constexpr int NoteG = 7;
    constexpr int NoteA = 9;
    constexpr int NoteB = 11;
    constexpr int OctaveBase = 12;
    constexpr int SharpOffset = 1;
    constexpr int FlatOffset = -1;

    std::string ToLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    std::string Trim(const std::string& str)
    {
        const auto start = str.find_first_not_of(" \t\r\n");
        if (std::string::npos == start)
        {
            return "";
        }
        const auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    double NoteNameToHz(const std::string& noteName)
    {
        if (noteName.empty())
        {
            return 0.0;
        }

        int semitone = -1;
        const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(noteName[0])));

        switch (letter)
        {
        case 'C': semitone = NoteC; break;
        case 'D': semitone = NoteD; break;
        case 'E': semitone = NoteE; break;
        case 'F': semitone = NoteF; break;
        case 'G': semitone = NoteG; break;
        case 'A': semitone = NoteA; break;
        case 'B': semitone = NoteB; break;
        default: return 0.0;
        }

        size_t pos = 1;
        if (pos < noteName.size() && ('s' == noteName[pos] || '#' == noteName[pos]))
        {
            semitone += SharpOffset;
            ++pos;
        }
        else if (pos < noteName.size() && ('b' == noteName[pos]))
        {
            semitone += FlatOffset;
            ++pos;
        }

        if (pos >= noteName.size())
        {
            return 0.0;
        }

        const int octave = noteName[pos] - '0';
        const int midiNote = (octave + 1) * OctaveBase + semitone;

        return A4Frequency * std::pow(2.0, (static_cast<double>(midiNote) - static_cast<double>(A4NoteNumber)) / SemitonesPerOctave);
    }

    bool ParseInstrument(const std::string& token, ParsedCommand& cmd)
    {
        const std::string lower = ToLower(token);

        if ("kick" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Kick; cmd.InstrumentKey = "kick"; return true; }
        if ("snare" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Snare; cmd.InstrumentKey = "snare"; return true; }
        if ("hat" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::ClosedHat; cmd.InstrumentKey = "hat"; return true; }
        if ("ohat" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::OpenHat; cmd.InstrumentKey = "ohat"; return true; }
        if ("clap" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Clap; cmd.InstrumentKey = "clap"; return true; }
        if ("kick808" == lower || "808kick" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Kick808; cmd.InstrumentKey = "kick808"; return true; }
        if ("snare808" == lower || "808snare" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Snare808; cmd.InstrumentKey = "snare808"; return true; }
        if ("hat808" == lower || "808hat" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Hat808; cmd.InstrumentKey = "hat808"; return true; }
        if ("ohat808" == lower || "808ohat" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::OpenHat808; cmd.InstrumentKey = "ohat808"; return true; }
        if ("clap808" == lower || "808clap" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Clap808; cmd.InstrumentKey = "clap808"; return true; }
        if ("ride" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Ride; cmd.InstrumentKey = "ride"; return true; }
        if ("rim" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Rim; cmd.InstrumentKey = "rim"; return true; }
        if ("tom" == lower) { cmd.Instrument = InstrumentType::Drum; cmd.Drum = DrumType::Tom; cmd.InstrumentKey = "tom"; return true; }
        if ("sub" == lower) { cmd.Instrument = InstrumentType::Synth; cmd.Waveform = WaveformType::Sub; return true; }
        if ("wub" == lower) { cmd.Instrument = InstrumentType::Synth; cmd.Waveform = WaveformType::Wub; return true; }
        if ("chip" == lower) { cmd.Instrument = InstrumentType::Synth; cmd.Waveform = WaveformType::Chip; return true; }
        if ("sine" == lower) { cmd.Instrument = InstrumentType::Synth; cmd.Waveform = WaveformType::Sine; return true; }
        if ("square" == lower) { cmd.Instrument = InstrumentType::Synth; cmd.Waveform = WaveformType::Square; return true; }
        if ("sawtooth" == lower) { cmd.Instrument = InstrumentType::Synth; cmd.Waveform = WaveformType::Sawtooth; return true; }
        if ("triangle" == lower) { cmd.Instrument = InstrumentType::Synth; cmd.Waveform = WaveformType::Triangle; return true; }

        return false;
    }

    std::unique_ptr<std::vector<std::string>> SplitDot(const std::string& str)
    {
        auto pParts = std::make_unique<std::vector<std::string>>();
        std::istringstream stream(str);
        std::string part;
        while (std::getline(stream, part, '.'))
        {
            if (!part.empty())
            {
                pParts->push_back(part);
            }
        }
        return pParts;
    }

    std::string ExtractParenContents(const std::string& str)
    {
        const auto openPos = str.find('(');
        const auto closePos = str.rfind(')');
        if (std::string::npos != openPos && std::string::npos != closePos && closePos > openPos)
        {
            return str.substr(openPos + 1, closePos - openPos - 1);
        }
        return "";
    }

    constexpr int DefaultDurationSteps = 1;

    bool IsDigits(const std::string& str)
    {
        for (const char c : str)
        {
            if (!std::isdigit(static_cast<unsigned char>(c))) { return false; }
        }
        return !str.empty();
    }

    void ParsePlayArgs(const std::string& parenContents, uint32_t& outMods, int& outDuration)
    {
        outMods = Mod::None;
        outDuration = DefaultDurationSteps;
        std::istringstream stream(parenContents);
        std::string token;
        bool durationFound = false;
        while (std::getline(stream, token, ','))
        {
            const std::string trimmed = Trim(token);
            if (trimmed.empty()) { continue; }
            const std::string lower = ToLower(trimmed);

            if ("detune" == lower) { outMods |= Mod::Detune; }
            else if ("vibrato" == lower) { outMods |= Mod::Vibrato; }
            else if ("tremolo" == lower) { outMods |= Mod::Tremolo; }
            else if ("pwm" == lower) { outMods |= Mod::Pwm; }
            else if ("distortion" == lower) { outMods |= Mod::Distortion; }
            else if ("adsr" == lower) { outMods |= Mod::Adsr; }
            else if ("bitcrush" == lower || "crush" == lower) { outMods |= Mod::Bitcrush; }
            else if (!durationFound && IsDigits(trimmed)) { outDuration = std::stoi(trimmed); durationFound = true; }
        }
    }

    std::string ExtractBeforeParen(const std::string& str)
    {
        const auto openPos = str.find('(');
        if (std::string::npos != openPos)
        {
            return str.substr(0, openPos);
        }
        return str;
    }
}

namespace CommandParser
{
    ParsedCommand Parse(const std::string& input)
    {
        ParsedCommand cmd;
        const std::string trimmed = Trim(input);

        if (trimmed.empty())
        {
            cmd.Type = CommandType::Invalid;
            return cmd;
        }

        const std::string lower = ToLower(trimmed);

        if ("help" == lower)
        {
            cmd.Type = CommandType::Help;
            return cmd;
        }
        if ("clear" == lower)
        {
            cmd.Type = CommandType::Clear;
            return cmd;
        }
        if ("exit" == lower || "quit" == lower)
        {
            cmd.Type = CommandType::Exit;
            return cmd;
        }
        if ("stop()" == lower)
        {
            cmd.Type = CommandType::StopAll;
            return cmd;
        }
        if ("binds" == lower)
        {
            cmd.Type = CommandType::Binds;
            return cmd;
        }
        if ("bpm.get()" == lower)
        {
            cmd.Type = CommandType::GetBpm;
            return cmd;
        }

        // bpm.set(120)
        if (lower.substr(0, 8) == "bpm.set(")
        {
            const std::string contents = ExtractParenContents(trimmed);
            if (!contents.empty())
            {
                cmd.Type = CommandType::SetBpm;
                cmd.BpmValue = std::stoi(contents);
                return cmd;
            }
        }

        // stop(kick) or stop(sine.C4)
        if (lower.substr(0, 5) == "stop(")
        {
            const std::string contents = Trim(ExtractParenContents(trimmed));
            if (!contents.empty())
            {
                cmd.Type = CommandType::Stop;
                cmd.InstrumentKey = ToLower(contents);
                return cmd;
            }
        }

        // bind(1, sawtooth.C4.play(4, detune))
        if (lower.substr(0, 5) == "bind(")
        {
            const std::string contents = ExtractParenContents(trimmed);
            const auto commaPos = contents.find(',');
            if (std::string::npos != commaPos)
            {
                const std::string slotStr = Trim(contents.substr(0, commaPos));
                const std::string boundCmd = Trim(contents.substr(commaPos + 1));
                if (IsDigits(slotStr) && !boundCmd.empty())
                {
                    cmd.Type = CommandType::Bind;
                    cmd.BindSlot = std::stoi(slotStr);
                    cmd.BindCommand = boundCmd;
                    return cmd;
                }
            }
            cmd.Type = CommandType::Invalid;
            cmd.ErrorMessage = "Usage: bind(1, sawtooth.C4.play(4, detune))";
            return cmd;
        }

        // loop(kick, 1, 5, 9, 13) or loop(sine.C4, 1, 3, 5)
        if (lower.substr(0, 5) == "loop(")
        {
            const std::string contents = ExtractParenContents(trimmed);
            std::istringstream stream(contents);
            std::string token;

            // First token is the instrument (may contain dots)
            if (std::getline(stream, token, ','))
            {
                const std::string instrStr = Trim(token);
                auto pDotParts = SplitDot(instrStr);

                if (!pDotParts->empty() && ParseInstrument((*pDotParts)[0], cmd))
                {
                    if (InstrumentType::Synth == cmd.Instrument && pDotParts->size() >= 2)
                    {
                        cmd.FrequencyHz = NoteNameToHz((*pDotParts)[1]);
                        cmd.InstrumentKey = ToLower((*pDotParts)[0]) + "." + (*pDotParts)[1];
                    }

                    // Remaining tokens: numbers are steps, strings are modulations
                    while (std::getline(stream, token, ','))
                    {
                        const std::string trimmedToken = Trim(token);
                        if (trimmedToken.empty()) { continue; }
                        const std::string lowerToken = ToLower(trimmedToken);

                        if ("detune" == lowerToken) { cmd.Modulation |= Mod::Detune; }
                        else if ("vibrato" == lowerToken) { cmd.Modulation |= Mod::Vibrato; }
                        else if ("tremolo" == lowerToken) { cmd.Modulation |= Mod::Tremolo; }
                        else if ("pwm" == lowerToken) { cmd.Modulation |= Mod::Pwm; }
                        else if ("distortion" == lowerToken) { cmd.Modulation |= Mod::Distortion; }
                        else if ("adsr" == lowerToken) { cmd.Modulation |= Mod::Adsr; }
                        else if ("bitcrush" == lowerToken || "crush" == lowerToken) { cmd.Modulation |= Mod::Bitcrush; }
                        else if (IsDigits(lowerToken)) { cmd.pSteps->push_back(std::stoi(trimmedToken)); }
                    }

                    if (!cmd.pSteps->empty())
                    {
                        cmd.Type = CommandType::Loop;
                        return cmd;
                    }
                }
            }

            cmd.Type = CommandType::Invalid;
            cmd.ErrorMessage = "Usage: loop(instrument, step1, step2, ...)";
            return cmd;
        }

        // instrument.note.play() or drum.play()
        const std::string beforeParen = ExtractBeforeParen(trimmed);
        auto pParts = SplitDot(beforeParen);

        if (pParts->empty())
        {
            cmd.Type = CommandType::Invalid;
            cmd.ErrorMessage = "Unknown command. Type 'help' for usage.";
            return cmd;
        }

        if (!ParseInstrument((*pParts)[0], cmd))
        {
            cmd.Type = CommandType::Invalid;
            cmd.ErrorMessage = "Unknown instrument: " + (*pParts)[0];
            return cmd;
        }

        if (InstrumentType::Synth == cmd.Instrument)
        {
            if (pParts->size() < 2)
            {
                cmd.Type = CommandType::Invalid;
                cmd.ErrorMessage = "Synth needs a note: " + (*pParts)[0] + ".C4.play()";
                return cmd;
            }

            cmd.FrequencyHz = NoteNameToHz((*pParts)[1]);
            cmd.InstrumentKey = ToLower((*pParts)[0]) + "." + (*pParts)[1];

            if (cmd.FrequencyHz <= 0.0)
            {
                cmd.Type = CommandType::Invalid;
                cmd.ErrorMessage = "Invalid note: " + (*pParts)[1];
                return cmd;
            }
        }

        // Check for play(...) at the end — with optional duration + modulation args
        if (lower.find("play(") != std::string::npos || lower.find("play()") != std::string::npos)
        {
            const std::string playContents = ExtractParenContents(trimmed);
            ParsePlayArgs(playContents, cmd.Modulation, cmd.DurationSteps);
            cmd.Type = CommandType::PlayOnce;
            return cmd;
        }

        cmd.Type = CommandType::Invalid;
        cmd.ErrorMessage = "Did you mean: " + cmd.InstrumentKey + ".play()?";
        return cmd;
    }
}
