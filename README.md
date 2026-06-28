# BeepSongs

A small Windows audio suite written in modern C++. It synthesizes music from
scratch with the Win32 `waveOut` API — sine-wave additive synthesis, no
external audio libraries — and stores songs as a compact binary format that is
**memory-mapped and played straight off disk**.

The repository is a single Visual Studio solution containing three programs that
share a common audio/data core.

## Components

| Project | Type | What it does |
| --- | --- | --- |
| **BeepSongs** | Console | The player. Discovers `.beep` files in a `Songs` folder beside the executable and plays the one you pick. Songs are memory-mapped, so playback reads the note data directly from disk with no parsing or copying. |
| **MidiViewer** | Win32 GUI | Opens a MIDI file, draws a piano-roll, lets you audition and filter individual tracks, then installs the result into BeepSongs as a `.beep` file in one click. Owns the MIDI-parsing / note-conversion / `.beep`-writing pipeline. |
| **BeepMaker** | Console | An interactive live beat/synth REPL — type commands at a `beep>` prompt to trigger sounds, set up loops, change BPM, and bind patterns to `Ctrl+0`–`Ctrl+9`. |

## The `.beep` song format

A `.beep` file is a fixed header followed by a raw array of note records. The
note records are a byte-for-byte image of the in-memory `TimedNote` struct, so
the runtime maps the file and reads the array in place — zero parsing, zero
copies.

```
┌──────────────────────────────┐
│ FileHeader (96 bytes)        │  magic 'BEEP', version, note count,
│                              │  total duration, notes offset/stride,
│                              │  source BPM, 64-byte display name
├──────────────────────────────┤
│ TimedNote[NoteCount]         │  { StartMs, DurationMs, FrequencyHz, Velocity }
│   24 bytes each              │  (8-byte aligned for the doubles)
└──────────────────────────────┘
```

Because songs are data files rather than compiled-in code, adding a song does
not change the player or grow the executable — the menu is built at runtime from
whatever `.beep` files are present.

## How the sound is made

This project began as an experiment with the Win32 [`Beep()`](https://learn.microsoft.com/windows/win32/api/utilapiset/nf-utilapiset-beep)
function — the old kernel-level call that drives the system speaker. `Beep(frequency, duration)`
is trivial to use, but that is all it does: one fixed pitch, one duration, a
blocking call, a buzzy square wave, and no way to overlap notes or shape their
volume. To get real control over **tone, pitch, and timing**, BeepSongs drops
down to the Windows multimedia `waveOut` API and synthesizes the audio one
sample at a time, then hands raw PCM to the sound card.

**1. Describe the format.** A `WAVEFORMATEX` tells the device what the samples
look like — here, mono 16-bit signed PCM at 44.1 kHz:

```cpp
WAVEFORMATEX fmt = {};
fmt.wFormatTag      = WAVE_FORMAT_PCM;
fmt.nChannels       = 1;       // mono
fmt.nSamplesPerSec  = 44100;   // 44.1 kHz
fmt.wBitsPerSample  = 16;      // signed 16-bit samples
fmt.nBlockAlign     = nChannels * wBitsPerSample / 8;
fmt.nAvgBytesPerSec = nSamplesPerSec * nBlockAlign;
```

**2. Generate the wave.** Each note is a sine wave. For frequency *f*, sample
*i* is:

```
sample[i] = amplitude · sin(2π · f · i / sampleRate)
```

The code precomputes a per-sample phase step (`2π·f / sampleRate`) and walks it
across the note's duration. Because *f* is a plain `double`, **pitch** is
continuous — any frequency, not the integer-Hz steps of `Beep`. A short linear
fade-in/out (~200 samples) is applied to each note's amplitude so notes start
and stop without an audible click.

**3. Mix for polyphony.** `Beep` can only sound one tone at a time. Here,
overlapping notes (chords, harmonies) are summed into a wider floating-point mix
buffer, each shaped by an exponential decay envelope, and the result is
peak-limited to stay inside the 16-bit range before being rounded back to
`short` samples. That sum is what gives the **tone** its body and lets multiple
voices play at once.

**4. Send it to the device.** The finished PCM is wrapped in a `WAVEHDR` and
pushed out: `waveOutOpen → waveOutPrepareHeader → waveOutWrite → waveOutClose`.
The polyphonic player *streams* rather than rendering the whole song up front —
it keeps a handful of `WAVEHDR` chunk buffers in flight and refills each one as
the device signals (via an event callback) that it has finished, so playback
starts immediately and **timing** stays sample-accurate across the whole piece.
`Ctrl+C` calls `waveOutReset` to stop mid-stream.

The walkthrough above describes the polyphonic streaming path (`PlayTimedSong`),
which is what every bundled song actually uses — so the mixing, decay-envelope,
and streaming details match what you really hear. A simpler monophonic helper
(`PlaySong`) also exists, rendering one note after another into a single buffer
with the fade but no mixing or decay.

## Building

**Requirements:** Visual Studio 2026 (v143/v145 toolset), Windows 10+ SDK. The
projects target **x64**, **C++17**, and build clean under **`/W4 /WX`**
(warnings as errors).

Open `BeepSongs.sln` in Visual Studio and build, or from a Developer prompt:

```sh
msbuild BeepSongs.sln /p:Configuration=Release /p:Platform=x64
```

All three projects output to a shared directory:

```
Build/Release/      BeepSongs.exe, MidiViewer.exe, BeepMaker.exe
Build/Debug/
```

BeepSongs' `Songs` asset folder is copied next to the executable automatically
as a post-build step, so `Build/Release/` is self-contained and runnable.

## Usage

**Play songs** — run `BeepSongs.exe`, choose a number from the menu, and press
`Ctrl+C` to stop playback (it returns to the menu rather than quitting).

**Convert a MIDI into a song** — run `MidiViewer.exe`, open a `.mid`, check the
tracks you want, audition with **Play Checked**, optionally apply filters
(monophonic/skyline, clamp to audible range, transpose), then **Export → Install
to BeepSongs** to drop a `.beep` into the `Songs` folder. It is bundled into
BeepSongs on the next build.

**Live beat maker** — run `BeepMaker.exe` and type `help` at the `beep>` prompt
for the command list.

## Adding your own songs

1. Find or export a MIDI file and drop it in `Midi/` (a convenience drop-folder;
   `.mid` files there are git-ignored).
2. Convert it with **MidiViewer** (open, select tracks, Install to BeepSongs).
3. The resulting `.beep` lands in `BeepSongs/Songs/`; rebuild BeepSongs to bundle it.

## Project structure

```
BeepSongs/                  Solution root
├── BeepSongs.sln
├── BeepSongs/              Player project
│   ├── Include/            Player + format headers (TimedNote, BeepFormat, MappedSong, ...)
│   ├── Source/             Player implementation
│   └── Songs/              .beep song assets, loaded at runtime
├── MidiViewer/             MIDI viewer + converter + installer (Win32 GUI)
├── BeepMaker/              Live beat/synth REPL (console)
├── Midi/                   Drop-folder for your MIDI inputs (*.mid git-ignored)
└── Build/                  Build output (git-ignored)
```

(`BeepFormat.h` / `TimedNote.h` in `BeepSongs/Include` are also shared by MidiViewer,
which references them for the `.beep` writer.)
