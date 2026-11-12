# Collaboration

How two people work on Blackwater without stepping on each other.

## Work areas

Good handoffs have a clear boundary: their own folder, a small agreed API, and little
reason to edit someone else's files.

| Area | What | Boundary | When |
|---|---|---|---|
| **Audio** | XAudio2 sound engine | `Blackwater.Audio` — [spec](Audio.md) | now |
| **Physics / collision** | 2D collision, walkability, queries | `Blackwater.Physics` — [spec](Physics.md) | M7 |
| **CI** | GitHub Actions: build + run tests on every pull request | `.github/workflows/` | M4 |
| **Debug overlay / profiler** | On-screen frame time, counters, toggle with a key | Reads engine data, never changes it | after M3 |
| **Lighting & shading** | Directional light → Blinn-Phong → shadows | Shaders + a lighting constant buffer | M3 → M5 |
| **UI / HUD** | Health bars, ability hotbar, menus | Draws 2D on top of the frame | M8 |
| **Asset pipeline** | Validate / convert models and textures before the engine loads them | Separate tool `.exe` | M5 |
| **Editor** | `Blackwater.Editor` (WPF / C#) | Its own .NET project | later |
| **Lua scripting** | Gameplay scripts, triggers | Embedded Lua behind a small API | later |
| **Particles / VFX** | Spell effects, dust, sparks | Its own renderer pass | after combat |
| **Gamepad** | XInput controller support | Feeds `Input` like the mouse does | anytime |
| **Save / load** | Serialise game state | Plain data, no renderer | M10 |
| **Logging** | Levels, file output, replaces `OutputDebugString` | Tiny, used by everyone | anytime |

## Workflow

- **Branch per feature**, merge through a **pull request**, the other person reviews.
- **`main` always builds.** Once CI exists, it enforces this.
- Small commits, messages that say *why*.
- Agree a module's public header *before* building its inside.
- **Trim passes:** at milestones, we go through the code and cut what isn't earning its
  place — over-long comments, speculative code, anything that reads as filler.

## Before the first outside contribution

- [ ] Pick a repo licence
- [ ] Protect `main` on GitHub (require a pull request)
