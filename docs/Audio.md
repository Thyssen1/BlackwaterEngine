# Blackwater.Audio — Sound Engine

> Owner: *(friend)* · Target: `Blackwater.Audio` (static lib) · Status: API stub, no sound yet

## Scope

Play sounds for a single-player 2.5D game: one-shot effects, looping music, voice lines,
with a volume per category. Nothing more until the game asks for it.

## Public API

Lives in [`include/Blackwater/Audio/AudioEngine.h`](../Blackwater.Audio/include/Blackwater/Audio/AudioEngine.h).
Agreed shape — change it together, not alone:

| Call | Purpose |
|---|---|
| `LoadSound(path, category)` → `SoundHandle` | Load a file; `kInvalidSound` on failure |
| `Play(sound, volume)` | Play once |
| `SetCategoryVolume(category, volume)` | `Music`, `Effects`, `Voice` — 0..1 |
| `SetMasterVolume(volume)` | 0..1 |
| `Update()` | Once per frame: recycle finished voices, feed streams |

The backend is hidden behind `pImpl`, so internals can change freely without touching
anyone else's code.

## Backend: miniaudio

The engine targets **Windows and macOS** (DESIGN.md §5), so the backend must run on both.

- **[miniaudio](https://miniaud.io)** — single header, public domain / MIT-0. It wraps
  WASAPI on Windows and Core Audio on macOS, so the module is written **once**.
- Graph: sounds → one **sound group per category** → the engine's endpoint. Category volume
  = that group's volume, which is why it also affects sounds already playing.
- miniaudio mixes on its own audio thread. Game code calls the API from the game thread;
  callbacks on the audio thread must not touch game state.
- It decodes WAV, FLAC and MP3 out of the box — no separate loader needed to get started.
  Writing a small WAV parser yourself is still a good exercise.

## Order of work

1. Initialise the miniaudio engine; fail cleanly (log, keep running) with no device.
2. Load sounds; one-shot playback.
3. Category groups + volumes.
4. Looping + **streaming** for music (don't load a whole track into memory).
5. Later: simple left/right panning from the hero's screen position. Full 3D positional
   audio is probably overkill for a fixed isometric camera.

Build and test on both Windows and macOS — this module has no reason to be Windows-only.

## Done when

The Sandbox plays a click on right-click and music on start, and the volumes work.

## Rules shared with the rest of the engine

RAII, check every result and report which call failed, no platform headers in the public
API. See [DESIGN.md §9](../DESIGN.md).
