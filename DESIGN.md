# Blackwater — Engine Design

> **Blackwater** is a small 2.5D isometric game engine for **Windows (Direct3D 11) and
> macOS (Metal)**. Until M5 it runs on Windows only.
> It exists to **learn how engines work** — every line is meant to be understood, not
> imported.
>
> The game built on it is **Trinity** (working name) — see [docs/Trinity.md](docs/Trinity.md).

---

## 1. The one rule

```
   Trinity  ──links──▶  Blackwater
   (game)                 (engine)
```

**The arrow points one way.** Engine code never `#include`s game code and never knows what
a hero, item or quest is. The build enforces it: the engine targets don't link `Trinity`,
so such an include simply won't resolve.

---

## 2. What we are building

- **2.5D isometric**: real 3D models, seen through a **fixed isometric camera** —
  orthographic projection, locked angle, **no rotation, no zoom**. (Diablo 3/4, Hades.)
- **Windows and macOS** (from M5). The game talks to a small renderer interface (RHI);
  each platform has a backend behind it — see §5.
- **Build what Trinity needs, nothing speculative.** No general-purpose engine.
- **No physics engine.** Trinity needs picking, grid collision and unit separation —
  a small collision module we write ourselves (see §6).
- Single-player. No networking.
- Every milestone ends in **something runnable**.

---

## 3. Targets

| Target | Type | Role | Status |
|---|---|---|---|
| `Blackwater.Core` | static lib | Platform, input, renderer, camera | ✅ |
| `Blackwater.Sandbox` | .exe | Engine test bench — try engine features with no game attached | ✅ |
| `Blackwater.Tests` | .exe (CTest) | Unit tests | planned (M4) |
| `Blackwater.Audio` | static lib | Sound engine — XAudio2 ([spec](docs/Audio.md)) | API stub |
| `Blackwater.Physics` | static lib | 2D collision & queries ([spec](docs/Physics.md)) | planned (M7) |
| `Trinity` | .exe | **The game.** Created when the first game-specific code appears | planned |
| `Blackwater.Editor` | WPF / .NET 10 | Tooling. MSBuild, outside CMake | parked |

**The Sandbox is not Trinity.** It is where engine features are exercised in isolation, and
it stays useful after Trinity exists.

### Why the engine is a *static* library

A DLL boundary is an ABI boundary, and C++ has no stable ABI. Static linking means STL
types, templates and exceptions cross freely between engine and game; no
`dllexport` decoration; the optimizer sees through the boundary; and one `.exe` with
nothing to copy. If the editor ever needs to call in, it gets a deliberately tiny
`extern "C"` DLL wrapping the static core — not an exported C++ engine.

### Layout

```
BlackwaterEngine/
├─ CMakeLists.txt / CMakePresets.json   the build (the only build files you edit)
├─ DESIGN.md                            this file
├─ docs/Trinity.md                      the game
├─ Blackwater.Core/
│   ├─ include/Blackwater/              public headers → #include <Blackwater/Window.h>
│   └─ src/                             implementation
├─ Blackwater.Audio/                    sound engine (API stub)
├─ Blackwater.Sandbox/                  engine test bench (src/, shaders/)
├─ Blackwater.Editor/                   WPF, parked
└─ build/  bin/  lib/                   GENERATED — gitignored, safe to delete
```

If a header isn't in `include/`, nothing outside the engine can reach it.

---

## 4. Build & run

CMake owns the build. Everything under `build/` (`.slnx`, `ZERO_CHECK`, `ALL_BUILD`,
`CMakeCache.txt`, …) is **generated output** — never edit it, delete it freely.

⚠️ The `cmake` on PATH (3.29) is too old for VS 2026. Use the bundled 4.3.1, or run
`winget install Kitware.CMake` once.

```powershell
$cm = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cm --preset vs                 # first time only (or after deleting build/)
& $cm --build --preset vs-debug   # build — reconfigures itself if a CMakeLists changed
.\bin\Debug\Blackwater.Sandbox.exe
```

- **Rider:** Settings → CMake → enable the **Ninja - Debug** profile. Run configurations
  appear for each executable target.
- Close the game window before building, or linking fails with `LNK1168`.
- Toolset is **`v145`** (VS 2026).

---

## 5. Rendering

### Cross-platform shape (M5)

```
        Game / Sandbox  (platform-neutral)
                │
        Blackwater RHI   ← buffers, pipelines, constants, draw, present
          ┌─────┴─────┐
     D3D11 backend   Metal backend (metal-cpp)
       Windows          macOS
```

| Concern | Windows | macOS |
|---|---|---|
| Rendering | D3D11 (today's code becomes this backend) | Metal via `metal-cpp` |
| Window + input | **SDL3** | **SDL3** |
| Shaders | HLSL | HLSL → SPIR-V → MSL at build time |
| Audio | miniaudio | miniaudio |

The cost, accepted on purpose: every rendering feature is written once per backend. That is
why the RHI lands **before** models, textures and animation.

**Dependencies** are no longer zero: SDL3, metal-cpp, miniaudio and the shader
cross-compile tools. Each one is a deliberate choice recorded here — nothing gets added
casually.

### Decisions

| Decision | Choice |
|---|---|
| API | **Direct3D 11** (Windows) / **Metal** (macOS), forward rendering |
| Camera | **Fixed isometric**: orthographic, yaw 45°, pitch ≈ 30–35° (tuned by eye), no zoom. Follows the hero once the world outgrows the screen |
| Math | **DirectXMath** — `XMMATRIX` for computation, `XMFLOAT4X4` for storage |
| Shaders | HLSL, **compiled at runtime** (`D3DCompile`); offline compilation later |
| Shading | One directional light (Lambert) → Blinn-Phong later |
| Models | **glTF 2.0** (`.glb`) — loader flips right-handed → left-handed on import |
| Textures | **WIC** (built into Windows) for PNG/JPEG |
| COM | **`ComPtr<T>` everywhere**, never a raw COM pointer |
| Debug | D3D11 debug layer in Debug builds; check with `GraphicsDevice::IsDebugLayerActive()` |

The debug layer needs Windows' **Graphics Tools** feature
(`Add-WindowsCapability -Online -Name "Tools.Graphics.DirectX~~~~0.0.1.0"`, elevated).
Don't test for it by filename — the 64-bit DLL is `d3d11_3SDKLayers.dll`, not
`D3D11SDKLayers.dll`.

---

## 6. Simulation

- **Fixed timestep, 60 Hz.** `OnUpdate` gets an exact, constant delta; `OnRender` gets
  `alpha` and interpolates anything that moves *continuously* between its previous and
  current state.
- **Interpolation is for drawing only.** Never feed an interpolated value back into the
  simulation — arrival checks, collision and range use the *current* simulation state.
- **Input edges are cleared per simulation step**, not per frame, so a click is seen
  exactly once.
- **Collision, not physics:** ray picking (done), grid walkability, circle-vs-circle unit
  separation, simple projectile tests. If rigid bodies are ever needed: Jolt Physics.

---

## 7. Modules & ownership

Parts with clean boundaries can be owned by different people. Each gets its own target,
its own folder, and a small public API agreed up front — then the inside is the owner's
call.

| Module | Owner | Notes |
|---|---|---|
| Core / renderer / camera | Christian | |
| **Audio** (`Blackwater.Audio`) | friend | [docs/Audio.md](docs/Audio.md) |
| **Physics** (`Blackwater.Physics`) | friend | [docs/Physics.md](docs/Physics.md) |
| Tests | shared | Everyone tests their own module |

All work areas, the workflow, and further ideas: [docs/Collaboration.md](docs/Collaboration.md).

---

## 8. Roadmap

| # | Milestone | Outcome |
|---|---|---|
| **M0** ✅ | Heartbeat | Window, D3D11 device + flip-model swap chain, fixed-timestep loop |
| **M1** ✅ | First geometry | Buffers, runtime HLSL, constant buffers, a spinning cube |
| **M2** ✅ | Camera & ground | Input, ground grid, screen→world picking (orbit camera — replaced in M3) |
| **M3** | **MVP: move a cube** | Fixed isometric camera; right-click → the cube walks there (straight line, interpolated); one directional light so the cube reads as a cube |
| **M4** | Tests & collaboration | `Blackwater.Tests` + CTest; README; `Blackwater.Audio` skeleton (done early); CI |
| **M5** | **Cross-platform** | RHI interface; D3D11 moved behind it; SDL3 window + input; Metal backend; macOS presets + CI. **The MVP runs on both** |
| **M6** | Models | glTF meshes, textures, materials. Your own model replaces the cube |
| **M7** | Animation | Skeletons, skinning, idle/walk clips |
| **M8** | World | Walkability grid, A\* pathfinding, collision, camera follow. **`Trinity` target is created** |
| **M9** | Combat | Ability hotbar (`1 2 3`, `Q W E D`), cooldowns, an enemy |
| **M10** | Party | Three heroes: selection and follow AI |
| **M11** | RPG & story | Stats, items, dialogue, quest flags, save/load |

Tests come before cross-platform on purpose: the RHI refactor moves a lot of code, and tests
are what prove nothing broke.

Audio runs alongside from M4. The editor stays parked until there is data worth editing.

---

## 9. Conventions

- **RAII everywhere.** No raw `new`/`delete`, no raw COM pointers.
- **Check every `HRESULT`** (`BW_CHECK`) and every Win32 failure (`BW_CHECK_WIN32`) — the
  report names the call, file and line.
- **Headers declare, sources define.** No `using namespace` in a header.
- **Source lists are explicit** in `CMakeLists.txt` — no `file(GLOB)`.

---

## 10. Open decisions

- [x] macOS support — **yes, from M5**: Metal backend, SDL3 for window + input on both

- [ ] Test framework for M4: **doctest** (single header) vs a hand-rolled one
- [ ] glTF parsing for M5: **cgltf** (single header, narrow exception to "no dependencies") vs hand-written
- [ ] Exact isometric pitch: true isometric (35.26°) vs game-style (~30°)
- [ ] Repo licence
- [ ] Final name for the game
