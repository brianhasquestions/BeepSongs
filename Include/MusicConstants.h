#pragma once

#include <Windows.h>

/// @file MusicConstants.h
/// @brief Defines standard musical note frequencies and duration constants
///        used throughout the BeepSongs application.

namespace Music
{
    /// @brief Standard note frequencies in Hertz (Hz) for octaves 4 and 5.
    namespace Frequency
    {
        constexpr DWORD Rest = 0;

        constexpr DWORD C4  = 262;
        constexpr DWORD Cs4 = 277;
        constexpr DWORD D4  = 294;
        constexpr DWORD Ds4 = 311;
        constexpr DWORD E4  = 330;
        constexpr DWORD F4  = 349;
        constexpr DWORD Fs4 = 370;
        constexpr DWORD G4  = 392;
        constexpr DWORD Gs4 = 415;
        constexpr DWORD A4  = 440;
        constexpr DWORD As4 = 466;
        constexpr DWORD B4  = 494;

        constexpr DWORD C5  = 523;
        constexpr DWORD Cs5 = 554;
        constexpr DWORD D5  = 587;
        constexpr DWORD Ds5 = 622;
        constexpr DWORD E5  = 659;
        constexpr DWORD F5  = 698;
        constexpr DWORD Fs5 = 740;
        constexpr DWORD G5  = 784;
        constexpr DWORD Gs5 = 831;
        constexpr DWORD A5  = 880;
        constexpr DWORD As5 = 932;
        constexpr DWORD B5  = 988;

        constexpr DWORD C6  = 1047;
        constexpr DWORD Cs6 = 1109;
        constexpr DWORD D6  = 1175;
        constexpr DWORD Ds6 = 1245;
        constexpr DWORD E6  = 1319;
        constexpr DWORD F6  = 1397;
        constexpr DWORD Fs6 = 1480;
        constexpr DWORD G6  = 1568;
    }

    /// @brief Note durations in milliseconds, calibrated for 120 BPM.
    namespace Duration120
    {
        constexpr DWORD Whole          = 2000;
        constexpr DWORD DottedHalf     = 1500;
        constexpr DWORD Half           = 1000;
        constexpr DWORD DottedQuarter  = 750;
        constexpr DWORD Quarter        = 500;
        constexpr DWORD DottedEighth   = 375;
        constexpr DWORD Eighth         = 250;
        constexpr DWORD Sixteenth      = 125;
    }

    /// @brief Note durations in milliseconds, calibrated for 150 BPM.
    namespace Duration150
    {
        constexpr DWORD Whole          = 1600;
        constexpr DWORD DottedHalf     = 1000;
        constexpr DWORD Half           = 800;
        constexpr DWORD DottedQuarter  = 600;
        constexpr DWORD Quarter        = 400;
        constexpr DWORD DottedEighth   = 300;
        constexpr DWORD Eighth         = 200;
        constexpr DWORD Sixteenth      = 100;
    }

    /// @brief Note durations in milliseconds, calibrated for 160 BPM.
    namespace Duration160
    {
        constexpr DWORD Whole          = 1500;
        constexpr DWORD DottedHalf     = 1125;
        constexpr DWORD Half           = 750;
        constexpr DWORD DottedQuarter  = 562;
        constexpr DWORD Quarter        = 375;
        constexpr DWORD DottedEighth   = 281;
        constexpr DWORD Eighth         = 187;
        constexpr DWORD Sixteenth      = 94;
    }

    /// @brief Pause in milliseconds inserted between consecutive notes
    ///        to provide audible separation.
    constexpr DWORD NoteGapMs = 20;
}
