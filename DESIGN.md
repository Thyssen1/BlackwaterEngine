# Blackwater — Engine Design

> **Blackwater** is a 3D RPG engine for Windows, built with Win32 + Direct3D 11.
> It is written primarily to **learn how engines work** — every line is meant to be
> understood, not imported.
>
> The game built on it is **Trinity** (working name) — see [docs/Trinity.md](docs/Trinity.md).

---

## 1. The engine / game boundary

This is the most important rule in the project:

```
   Trinity  ──links──▶  Blackwater.Core
   (game)                  (engine)
```

**The arrow only points one way.** Engine code must never `#include` a Trinity header,
never reference a hero, an item, or a quest. If it ever does, Blackwater stops being an
engine and becomes "the thing Trinity is made of."

The compiler enforces this: `Blackwater.Core` does not link `Trinity`, so an engine
`#include` of game code simply won't resolve.

---

## 2. Goals & non-goals

**Goals**
- Learn engine and graphics architecture by **hand-rolling the interesting systems** —
  the render loop, the D3D11 pipeline, the scene representation, animation, pathfinding.
- Keep a hard **data-driven boundary** between the runtime and the editor.
- Every milestone ends in **something runnable**. Momentum matters more than elegance.

**Non-goals (for now)**
- Cross-platform. Windows/MSVC/x64 only — D3D11 and WPF are both Windows-bound.
- A general-purpose engine. Build what Trinity needs, and nothing speculative.
- Physics beyond what movement and picking require. No rigid-body simulation.
- Networking / multiplayer. Trinity is single-player.

---

## 3. Architecture

| Target | Type | Role |
|---|---|---|
| `Blackwater.Core` | **STATIC** lib (C++20) | The engine: platform, renderer, scene, animation, pathfinding |
| `Blackwater.Sandbox` | **WinMain .exe** | Engine test harness. Small, throwaway, always useful |
| `Trinity` | **WinMain .exe** | The game. Links the engine. Doesn't exist yet |
| `Blackwater.Interop` | SHARED **.dll** | Thin `extern "C"` ABI so the editor can P/Invoke. Later |
| `Blackwater.Editor` | WPF / .NET 10 | Tooling. **MSBuild, outside CMake** — CMake can't build XAML |

### Why Core is a STATIC library

A DLL boundary is an **ABI boundary**, and C++ has no stable ABI. Static linking avoids
the entire category:

- No `__declspec(dllexport/dllimport)` decoration on every public symbol.
- STL types (`std::string`, `std::vector`) can appear in engine APIs safely. Across a DLL
  they cannot — Debug and Release change `std::vector`'s layout, and allocating in one
  module while freeing in another corrupts the heap.
- Templates and `inline` code work normally instead of needing explicit instantiation.
- Exceptions and RTTI cross freely within one module.
- The optimizer sees through the boundary: inlining and whole-program optimization.
- One `.exe`. No "DLL not found", no copy step, F5 just works.

DLLs buy hot-reload, plugins, and independent patching — none of which we need. When the
**editor** needs to call in, it gets `Blackwater.Interop`: a deliberately tiny C ABI
(primitives and opaque handles only) wrapping the static core. A narrow ABI you design on
purpose beats accidentally exporting an entire C++ engine.

### Directory layout

```
BlackwaterEngine/
├─ CMakeLists.txt            root: toolchain, shared settings, subprojects
├─ CMakePresets.json         named configure/build presets
├─ DESIGN.md                 this file (engine)
├─ docs/Trinity.md           the game
│
├─ Blackwater.Core/          THE ENGINE — static library
│   ├─ CMakeLists.txt
│   ├─ include/Blackwater/   public headers  →  #include <Blackwater/Window.h>
│   └─ src/                  implementation + private headers
│
├─ Blackwater.Sandbox/       WinMain .exe — engine harness (M0 runs here)
├─ Blackwater.Editor/        WPF — own .csproj, outside CMake
└─ Trinity/                  the game — later
```

`include/` vs `src/` makes the public API surface physical: if a header isn't in
`include/`, nothing outside the engine can reach it. The `Blackwater/` subfolder acts as a
namespace so header names can't collide.

### Output layout

Everything runnable lands in `bin/<Config>/`, static libs in `lib/<Config>/`. The WPF
editor already writes to `bin/<Config>/`, so the engine binary sits beside it — that's how
the editor will find it at runtime. **Always build x64.**

---

## 4. Build

CMake owns the native build. The generated `.vcxproj` / `.sln` are **build output** — never
hand-edit them; edit `CMakeLists.txt` and regenerate.

⚠️ **Use the CMake bundled with VS 2026, not the one on PATH.**

| CMake | Max VS generator | Usable |
|---|---|---|
| 3.29.2 (`C:\Program Files\CMake`, on PATH) | Visual Studio 17 2022 | ❌ doesn't know VS 2026 |
| **4.3.1** (bundled in VS 2026) | **Visual Studio 18 2026** | ✅ |

```
C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\
    CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

(A Ninja is bundled alongside it.) Opening the folder directly in VS 2026 also works — it
uses its own CMake and reads `CMakePresets.json` automatically.

**Toolset:** VS 2026 registers its platform toolset as **`v145`** (not `v180` — that's the
MSBuild folder version). `v143` is VS 2022 and is not installed here.

---

## 5. Rendering

**Direct3D 11**, forward rendering, true 3D meshes.

| Decision | Choice | Why |
|---|---|---|
| API | **D3D11** | You write the renderer yourself. Native Windows, excellent tooling (PIX, RenderDoc), far gentler than D3D12/Vulkan |
| Math | **DirectXMath** | SIMD, ships with the SDK, no dependency |
| Pipeline | **Forward** | Simple, correct, plenty for a party-scale RPG. Deferred is a later option |
| Shading | **Blinn-Phong → PBR** | Start crude and understandable; upgrade once it renders at all |
| Models | **glTF 2.0** | Open, well-documented, good exporters, skinning built in |
| COM | **`ComPtr<T>` everywhere** | RAII refcounting. Never a raw COM pointer |
| Debug | **D3D11 debug layer in Debug builds** | Catches API misuse immediately; gated behind `BW_DEBUG` |

⚠️ The debug layer lives in Windows' **Graphics Tools** optional feature
(`D3D11SDKLayers.dll`). Without it, `D3D11CreateDevice` *fails* when the debug
flag is set, so `GraphicsDevice` retries without it and logs a note to the
debugger. Install it via **Settings → System → Optional features → Graphics
Tools** to get validation and COM leak reports.

The camera is **WC3-style**: fixed-ish pitch looking down at the party, free rotation and
zoom. It reads as isometric without being an orthographic 2D fake.

---

## 6. Engine subsystems

Roughly in dependency order:

| Layer | System | Notes |
|---|---|---|
| Platform | Win32 window, message pump, input | Foundation |
| Core | Fixed-timestep loop, timing, logging, asserts | Deterministic simulation |
| RHI | D3D11 device, swap chain, RTV, depth-stencil, states | M0 |
| Renderer | Buffers, shaders, constant buffers, MVP transforms | M1 |
| Camera | Orbit camera, view/projection, screen→world ray | M2 |
| Assets | glTF mesh + texture loading, material system | M3 |
| Animation | Skeleton, skinning, clips, blending | M4 |
| Scene | Transform hierarchy, culling, draw submission | M4–M6 |
| Navigation | Nav grid, **A\*** pathing, steering | M5 |
| Entities | A simple **ECS** | M6 |
| Gameplay hooks | Ability/cooldown framework, stats, inventory | Engine-side *mechanisms*; Trinity supplies the *content* |
| Narrative | Dialogue-tree runtime, flag/variable store | M9 |
| Persistence | Serialization, save/load | M10 |
| Pipeline | JSON data loading, asset registry | Throughout |

---

## 7. The editor

**One WPF app with dockable panels** (AvalonDock), not separate tools. It contains **no
game logic** — it reads and writes JSON that the runtime consumes.

**Spatial editors** (need a live 3D viewport): World/Scene, **Terrain** (heightmap
sculpt + texture paint, WC3 World Editor style), Entity/Prefab placer, Trigger/Region.

**Data editors** (tables and forms): Dialogue, Quest, Item/Loot, Ability/Skill,
Material, Script (Lua).

**Trigger ↔ Script:** the trigger editor is a friendly front-end that *generates* Lua, the
way WC3's did. The script editor is the raw view of the same system.

**Later:** VFX/particles, audio banks, localization/string table, AI behaviour, cutscene
timeline, asset browser.

### Docking shell

```
┌──────────────────────────────────────────────────────────────────────────┐
│  File  Edit  View  Tools  Build  Window  Help            [▢] Play ▶       │
├───────────────┬──────────────────────────────────────────┬───────────────┤
│  HIERARCHY    │                                          │  INSPECTOR    │
│  (scene tree) │                                          │  (properties  │
│  ┌─ Scene     │             VIEWPORT                      │   of selected)│
│  │  ├ Terrain │       (live D3D11 surface, shared         │               │
│  │  ├ Entities│        into WPF via D3DImage)             │  Name  [____] │
│  │  └ Lights  │                                          │  Pos  [_][_][_]│
│  │            │    ◇ grid, gizmos, sculpt brush           │  ...          │
│  TOOLBOX      │                                          │               │
│  ◻ Select     │                                          │               │
│  ◻ Sculpt     │                                          │               │
│  ◻ Paint      ├──────────────────────────┬───────────────┤               │
│  ◻ Place      │  ASSET BROWSER           │  CONSOLE      │               │
│  ◻ Trigger    │  (meshes, textures,      │  build +      │               │
│               │   prefabs, materials)    │  runtime log  │               │
├───────────────┴──────────────────────────┴───────────────┴──────────────┤
│  Ready │ Scene: keep_01 │ x64 Debug │ ● Engine OK                        │
└──────────────────────────────────────────────────────────────────────────┘
```

Spatial editors are *modes* acting on the one viewport. Data editors open as document
tabs. **Play ▶** boots the runtime against the current scene.

---

## 8. Roadmap

Each milestone ends in something runnable. **Build an editor only once the engine has a
system that consumes its data** — never author content nothing can read.

> This list is longer than the original 2D plan. True 3D adds a mesh pipeline, skinning,
> and lighting — each a real subsystem. The early milestones stay deliberately crude
> (untextured geometry before glTF) so progress never blocks on art.

### Engine

| # | Milestone | Outcome |
|---|---|---|
| **M0** ✅ | **Heartbeat** | Win32 window + message pump; D3D11 device, flip-model swap chain, RTV, depth-stencil; fixed-timestep loop; clears to a colour |
| **M1** | **First geometry** | Vertex/index buffers, HLSL shaders, constant buffers, MVP via DirectXMath, depth testing. A cube spins |
| **M2** | **Camera & ground** | WC3-style orbit camera; a ground plane/terrain grid; screen→world ray |
| **M3** | **Meshes** | glTF loading, textures, samplers, a basic material + Blinn-Phong light |
| **M4** | **Animation** | Skeletons, skinning, animation clips, blending. A character idles and walks |
| **M5** | **Movement** | Right-click → ground pick → **A\*** on a nav grid → the character walks the path |
| **M6** | **Entities & party** | Minimal ECS; **three heroes**; selection; follow/party AI |
| **M7** | **Combat** | Ability hotbar (`1/2/3`, `QWED`), cooldowns, targeting, an enemy with health |
| **M8** | **RPG systems** | Stats, levelling, items, inventory, equipment |
| **M9** | **Narrative** | Dialogue trees + quest/story flags that remember choices |
| **M10** | **Persistence & tooling** | Save/load; wire the editor to author scenes and data |

### Editor (interleaved)

| Phase | Editors | Pairs with |
|---|---|---|
| **E0** | Docking shell + embedded viewport (`D3DImage`) | M2–M3 |
| **E1** | Terrain (sculpt/paint) + Scene editor | M3 |
| **E2** | Entity placer + Trigger editor | M6–M7 |
| **E3** | Item + Ability editors | M7–M8 |
| **E4** | Dialogue + Quest editors | M9 |
| **E5** | Material, VFX, audio, localization | ongoing |

---

## 9. Conventions

- **RAII everywhere.** No raw `new`/`delete`, no raw COM pointers. `ComPtr<T>` for COM,
  `std::unique_ptr` for owned heap objects.
- **Check every `HRESULT`.** A failure path must report *which* call failed, with the file
  and line — not a silent early return.
- **Headers declare, sources define.** No `using namespace` in a header, ever.
- **Localize from day one.** All display text is a key into a string table.
- **Data-drive abilities and items immediately.** It's what turns the editor into a real
  tool and makes a content-heavy RPG feasible solo.

---

## 10. Open decisions

- [x] Rendering API — **Direct3D 11**
- [x] `Blackwater.Core` linkage — **STATIC**
- [x] Build system — **CMake** (4.3.1, bundled with VS 2026)
- [x] Renderer style — **true 3D meshes** (WC3/WoW), not 2D sprites
- [ ] Editor viewport embedding: `D3DImage` (recommended — composites with WPF) vs
      `HwndHost` (separate native child window)
- [ ] Scripting language: **Lua** (recommended) vs C# vs a custom DSL
- [ ] Model format confirmation: glTF 2.0 vs FBX
- [ ] Final name for the game (currently *Trinity*)
- [ ] Whether to keep versioning `.idea/` (Rider config) — currently tracked
