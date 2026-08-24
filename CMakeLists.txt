cmake_minimum_required(VERSION 3.21)

project(BlackwaterEngine
    VERSION     0.1.0
    DESCRIPTION "Isometric ARPG engine (Win32 + D3D11)"
    LANGUAGES   CXX)

# ---------------------------------------------------------------------------
# Platform guard
#
# D3D11 and the Win32 API are Windows-only, and we rely on MSVC-specific
# compiler flags below. Fail here, clearly, rather than 400 lines into a
# compile on the wrong platform.
# ---------------------------------------------------------------------------
if(NOT WIN32)
    message(FATAL_ERROR "Blackwater targets Windows only (Win32 + D3D11).")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "Blackwater requires the MSVC toolchain.")
endif()

# ---------------------------------------------------------------------------
# Output layout
#
# Everything runnable lands in bin/, static libs in lib/.
#
# NOTE: the Visual Studio generator is *multi-config* -- it appends the
# configuration name automatically, so these become bin/Debug, bin/Release.
# That matches where Blackwater.Editor (WPF/MSBuild) already writes, which is
# how the editor will find the engine binary at runtime.
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")   # .exe, .dll
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")   # shared libs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")   # .lib (static)

# ---------------------------------------------------------------------------
# Shared compile settings
#
# An INTERFACE library has no sources and produces no binary. It exists purely
# to carry "usage requirements" -- settings that propagate to whatever links
# it. Every Blackwater target links this, so there is exactly one place to
# change a warning level or a global define.
# ---------------------------------------------------------------------------
add_library(Blackwater.Common INTERFACE)

target_compile_features(Blackwater.Common INTERFACE cxx_std_20)

target_compile_definitions(Blackwater.Common INTERFACE
    UNICODE                     # Win32 calls resolve to the -W (wide) entry points,
    _UNICODE                    #   e.g. CreateWindow -> CreateWindowW, not ...A.
    WIN32_LEAN_AND_MEAN         # Drop rarely-used chunks of windows.h (winsock, OLE, ...).
    NOMINMAX                    # Suppress the min/max *macros* that break std::min/max.
    STRICT                      # Make Win32 HANDLE types distinct instead of void*.
    $<$<CONFIG:Debug>:BW_DEBUG>  # Our own flag; gates the D3D11 debug layer later.
)

target_compile_options(Blackwater.Common INTERFACE
    /W4                 # High warning level (/Wall is unusably noisy on system headers).
    /permissive-        # Enforce standards conformance; rejects old MSVC-isms.
    /utf-8              # Treat source as UTF-8 *and* set the execution charset.
    /MP                 # Compile translation units in parallel.
    /Zc:__cplusplus     # Report the true __cplusplus value (MSVC lies without this).
    /Zc:preprocessor    # Use the conformant preprocessor.
)

# ---------------------------------------------------------------------------
# Subprojects
#
# Order matters only for dependencies: Sandbox links Core, so Core is defined
# first. Blackwater.Editor is deliberately absent -- WPF is MSBuild-only and
# CMake cannot build XAML. It keeps its own .csproj.
# ---------------------------------------------------------------------------
add_subdirectory(Blackwater.Core)
add_subdirectory(Blackwater.Sandbox)

# Make F5 in Visual Studio launch the sandbox rather than a random target.
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Blackwater.Sandbox)
