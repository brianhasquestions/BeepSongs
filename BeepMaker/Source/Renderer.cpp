#include "Renderer.h"
#include <iostream>
#include <iomanip>

namespace Renderer
{
    void PrintBanner()
    {
        std::cout << std::endl;
        std::cout << "  ========================================" << std::endl;
        std::cout << "           B E E P M A K E R" << std::endl;
        std::cout << "      Live-Coding Beat Maker REPL" << std::endl;
        std::cout << "  ========================================" << std::endl;
        std::cout << std::endl;
        std::cout << "  Type 'help' for commands." << std::endl;
        std::cout << std::endl;
    }

    void PrintHelp()
    {
        std::cout << std::endl;
        std::cout << "  --- Drums ---" << std::endl;
        std::cout << "  kick.play()            Kick drum" << std::endl;
        std::cout << "  snare.play()           Snare drum" << std::endl;
        std::cout << "  hat.play()             Closed hi-hat" << std::endl;
        std::cout << "  ohat.play()            Open hi-hat" << std::endl;
        std::cout << "  clap.play()            Clap" << std::endl;
        std::cout << "  kick808.play()         808 Kick (deep sub-bass)" << std::endl;
        std::cout << "  snare808.play()        808 Snare" << std::endl;
        std::cout << "  hat808.play()          808 Closed hat (metallic)" << std::endl;
        std::cout << "  ohat808.play()         808 Open hat (shimmer)" << std::endl;
        std::cout << "  clap808.play()         808 Clap (layered)" << std::endl;
        std::cout << "  ride.play()            Ride cymbal" << std::endl;
        std::cout << "  rim.play()             Rimshot" << std::endl;
        std::cout << "  tom.play()             Tom" << std::endl;
        std::cout << std::endl;
        std::cout << "  --- Bass Synths (need a note) ---" << std::endl;
        std::cout << "  sub.C2.play()          Sub-bass (sine + harmonics)" << std::endl;
        std::cout << "  wub.E2.play()          Wub bass (saw + harmonics)" << std::endl;
        std::cout << std::endl;
        std::cout << "  --- Synths ---" << std::endl;
        std::cout << "  sine.C4.play()         Sine wave at C4" << std::endl;
        std::cout << "  square.E4.play()       Square wave at E4" << std::endl;
        std::cout << "  sawtooth.G4.play()     Sawtooth at G4" << std::endl;
        std::cout << "  triangle.C5.play()     Triangle at C5" << std::endl;
        std::cout << std::endl;
        std::cout << "  --- Notes ---" << std::endl;
        std::cout << "  C D E F G A B  (sharp: Cs4 or C#4, flat: Cb4 or Eb4)" << std::endl;
        std::cout << "  Octaves: 2-7" << std::endl;
        std::cout << std::endl;
        std::cout << "  --- Loops (steps 1-16) ---" << std::endl;
        std::cout << "  loop(kick, 1, 5, 9, 13)       Loop on steps" << std::endl;
        std::cout << "  loop(sine.C4, 1, 3, 5, 7)     Synth loop" << std::endl;
        std::cout << "  stop(kick)                     Stop a loop" << std::endl;
        std::cout << "  stop()                         Stop all" << std::endl;
        std::cout << std::endl;
        std::cout << "  --- Tempo ---" << std::endl;
        std::cout << "  bpm.set(120)           Set BPM" << std::endl;
        std::cout << "  bpm.get()              Show BPM" << std::endl;
        std::cout << std::endl;
        std::cout << "  --- Modulation (add to play) ---" << std::endl;
        std::cout << "  sawtooth.C4.play(detune)            Supersaw (3 detuned voices)" << std::endl;
        std::cout << "  sine.E4.play(vibrato)               Pitch wobble" << std::endl;
        std::cout << "  square.G4.play(tremolo)             Volume pulsing" << std::endl;
        std::cout << "  square.A4.play(pwm)                 Pulse width modulation" << std::endl;
        std::cout << "  sawtooth.C4.play(distortion)        Overdriven/clipped" << std::endl;
        std::cout << "  sine.C4.play(adsr)                  Attack-Decay-Sustain-Release" << std::endl;
        std::cout << "  sawtooth.C4.play(detune, vibrato)   Combine multiple effects" << std::endl;
        std::cout << std::endl;
        std::cout << "  --- Binds (Ctrl+Num to trigger) ---" << std::endl;
        std::cout << "  bind(1, kick808.play())                Bind Ctrl+1" << std::endl;
        std::cout << "  bind(2, sawtooth.C4.play(4, detune))   Bind Ctrl+2" << std::endl;
        std::cout << "  binds                                  Show all binds" << std::endl;
        std::cout << std::endl;
        std::cout << "  --- Other ---" << std::endl;
        std::cout << "  clear                  Clear screen" << std::endl;
        std::cout << "  exit                   Quit" << std::endl;
        std::cout << std::endl;
    }

    void PrintStatus(const Pattern& pattern)
    {
        auto pLoops = pattern.GetActiveLoops();
        const int bpm = pattern.GetBpm();

        if (pLoops->empty())
        {
            std::cout << "  BPM: " << bpm << "  |  No active loops" << std::endl;
            return;
        }

        std::cout << "  BPM: " << bpm << "  |  Active loops:" << std::endl;

        for (const auto& loop : *pLoops)
        {
            std::cout << "    " << std::left << std::setw(12) << loop.first << " [";
            for (int s = 0; s < Sequencer::StepCount; ++s)
            {
                if (0 != s)
                {
                    std::cout << " ";
                }
                std::cout << (loop.second[static_cast<size_t>(s)] ? "X" : ".");
            }
            std::cout << "]" << std::endl;
        }
    }
}
