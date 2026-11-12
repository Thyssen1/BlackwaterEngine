# Blackwater

A small 2.5D isometric game engine for Windows (C++20, Win32, Direct3D 11), and
**Trinity**, the game built on it.

## Requirements

- Windows 10/11, x64
- **Visual Studio 2026** with *Desktop development with C++* (toolset `v145`)
- Optional: Windows **Graphics Tools** feature, for the D3D11 debug layer

## Build & run

```powershell
$cm = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cm --preset vs                # first time
& $cm --build --preset vs-debug
.\bin\Debug\Blackwater.Sandbox.exe
```

That is the CMake bundled with VS 2026 — a standalone CMake older than 4.x doesn't know
the VS 2026 generator. **Rider:** Settings → CMake → enable the *Ninja - Debug* profile.

## Docs

- [DESIGN.md](DESIGN.md) — engine design and roadmap
- [docs/Trinity.md](docs/Trinity.md) — the game
- [docs/Collaboration.md](docs/Collaboration.md) — work areas and workflow
- [docs/Audio.md](docs/Audio.md), [docs/Physics.md](docs/Physics.md) — module specs
