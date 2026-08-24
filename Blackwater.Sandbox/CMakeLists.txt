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

set(BLACKWATER_SANDBOX_SHADERS
    shaders/Basic.hlsl
)

# WIN32 means /SUBSYSTEM:WINDOWS -- a GUI application, so no console window is
# allocated and the entry point is wWinMain instead of main. Without it you get
# a console app, and the linker goes looking for main().
add_executable(Blackwater.Sandbox WIN32
    ${BLACKWATER_SANDBOX_SOURCES}
    ${BLACKWATER_SANDBOX_SHADERS}
)

# The shaders are listed above only so they appear in Solution Explorer. Visual
# Studio would otherwise hand any .hlsl file to FXC and try to compile it at
# build time -- but Blackwater compiles shaders at *runtime*, so that would be
# a second, redundant compile whose failures we would never see.
set_source_files_properties(${BLACKWATER_SANDBOX_SHADERS} PROPERTIES
    HEADER_FILE_ONLY  TRUE
    VS_TOOL_OVERRIDE  "None"
)

# PRIVATE because nothing links Sandbox -- it is the end of the chain, so
# there is nothing to propagate to. Core's PUBLIC settings (C++20, /W4, the
# Win32 defines) and its PRIVATE d3d11/dxgi still arrive here, because Core is
# a static library and forwards them to whoever does the final link.
target_link_libraries(Blackwater.Sandbox PRIVATE Blackwater.Core)

# bw::Shader resolves paths against the executable's own folder, so the .hlsl
# files have to be there. Copying after every build means editing a shader and
# rebuilding is enough -- no manual step to forget.
add_custom_command(TARGET Blackwater.Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:Blackwater.Sandbox>/shaders"
    COMMENT "Copying shaders next to Blackwater.Sandbox.exe"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${BLACKWATER_SANDBOX_SOURCES}
    ${BLACKWATER_SANDBOX_SHADERS}
)

set_target_properties(Blackwater.Sandbox PROPERTIES FOLDER "Sandbox")
