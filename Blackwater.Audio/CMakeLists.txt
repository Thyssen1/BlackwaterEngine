# ---------------------------------------------------------------------------
# Blackwater.Audio -- the sound engine, as a static library.
#
# Deliberately independent of Blackwater.Core: audio needs nothing from the
# renderer, window or camera. Spec: docs/Audio.md.
# ---------------------------------------------------------------------------

set(BLACKWATER_AUDIO_SOURCES
    include/Blackwater/Audio/AudioEngine.h
    src/AudioEngine.cpp
)

add_library(Blackwater.Audio STATIC ${BLACKWATER_AUDIO_SOURCES})

target_include_directories(Blackwater.Audio
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# miniaudio (header-only) is added once the backend lands -- see docs/Audio.md.
target_link_libraries(Blackwater.Audio
    PUBLIC
        Blackwater.Common
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${BLACKWATER_AUDIO_SOURCES})

set_target_properties(Blackwater.Audio PROPERTIES FOLDER "Engine")
