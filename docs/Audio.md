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

## Backend: XAudio2

- **XAudio2 2.9** ships with Windows 10+. Header `xaudio2.h`, link `xaudio2.lib` — no
  extra install, fits the "Windows APIs only" rule.
- Graph: **source voices** → one **submix voice per category** → **mastering voice**.
  Category volume = that submix's volume, which is why it also affects sounds already
  playing.
- XAudio2 runs its own audio thread. Game code calls the API from the game thread; the
  callbacks XAudio2 fires on its thread must not touch game state.
- Requires COM: `CoInitializeEx` before creating the engine.

## Order of work

1. Initialise XAudio2 + mastering voice; fail cleanly (log, keep running) with no device.
2. **WAV** loading — parse the RIFF header yourself; it's small and a good exercise.
3. One-shot playback with a **voice pool** (don't create/destroy voices per sound).
4. Category submixes + volumes.
5. Looping + **streaming** for music (don't load a whole track into memory).
6. Later: OGG/Opus (needs a decoder library — discuss first), simple left/right panning
   from the hero's screen position. Full 3D positional audio (X3DAudio) is probably
   overkill for a fixed isometric camera.

## Done when

The Sandbox plays a click on right-click and music on start, and the volumes work.

## Rules shared with the rest of the engine

RAII and `ComPtr<T>` for every COM object, every `HRESULT` checked (`BW_CHECK` lives in
Core — copy the pattern or we move `Check.h` somewhere both can use). See
[DESIGN.md §9](../DESIGN.md).
