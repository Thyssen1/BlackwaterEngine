# ---------------------------------------------------------------------------
# Blackwater.Sandbox -- engine test harness.
#
# A deliberately small executable whose only job is to drive the engine. M0
# lives here. It stays useful long after: somewhere to exercise one subsystem
# without dragging game content along.
# ---------------------------------------------------------------------------

set(BLACKWATER_SANDBOX_SOURCES
    src/Main.cpp
)

# WIN32 means /SUBSYSTEM:WINDOWS -- a GUI application, so no console window is
# allocated and the entry point is wWinMain instead of main. Without it you get
# a console app, and the linker goes looking for main().
add_executable(Blackwater.Sandbox WIN32 ${BLACKWATER_SANDBOX_SOURCES})

# PRIVATE because nothing links Sandbox -- it is the end of the chain, so
# there is nothing to propagate to. Core's PUBLIC settings (C++20, /W4, the
# Win32 defines) and its PRIVATE d3d11/dxgi still arrive here, because Core is
# a static library and forwards them to whoever does the final link.
target_link_libraries(Blackwater.Sandbox PRIVATE Blackwater.Core)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${BLACKWATER_SANDBOX_SOURCES})

set_target_properties(Blackwater.Sandbox PROPERTIES FOLDER "Sandbox")
