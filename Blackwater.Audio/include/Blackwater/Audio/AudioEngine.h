#pragma once

//
// The sound engine's public API. Spec and design notes: docs/Audio.md.
//
// The backend (XAudio2) is hidden behind a pointer to an incomplete type
// ("pImpl"), so no XAudio2 header ever reaches code that includes this file,
// and the internals can change without touching anyone else's code.
//

#include <cstdint>
#include <memory>

namespace bw
{
    enum class SoundCategory : uint8_t
    {
        Music,
        Effects,
        Voice,
        Count
    };

    /// Opaque id for a loaded sound. 0 is never a valid sound.
    using SoundHandle = uint32_t;
    inline constexpr SoundHandle kInvalidSound = 0;

    class AudioEngine final
    {
    public:
        AudioEngine();

        // Declared here, defined in the .cpp. unique_ptr<Impl> needs Impl to
        // be a complete type wherever it is destroyed -- and Impl is only
        // complete in the .cpp. A defaulted destructor here would not compile.
        ~AudioEngine();

        AudioEngine(const AudioEngine&)            = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;
        AudioEngine(AudioEngine&&)                 = delete;
        AudioEngine& operator=(AudioEngine&&)      = delete;

        /// Loads a sound file. Returns kInvalidSound on failure.
        [[nodiscard]] SoundHandle LoadSound(const wchar_t* path, SoundCategory category);

        /// Plays a loaded sound once. volume is 0..1, multiplied by the
        /// category and master volumes.
        void Play(SoundHandle sound, float volume = 1.0f);

        /// 0..1. Applies to every sound in the category, including ones
        /// already playing.
        void SetCategoryVolume(SoundCategory category, float volume);
        void SetMasterVolume(float volume);

        /// Call once per frame (housekeeping: finished voices, streaming).
        void Update();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
