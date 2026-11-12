#include "Blackwater/Audio/AudioEngine.h"

//
// Stub implementation: the API compiles and links, nothing makes sound yet.
// The miniaudio backend replaces these bodies -- see docs/Audio.md.
//

namespace bw
{
    struct AudioEngine::Impl
    {
        // miniaudio engine, one sound group per category, loaded sounds.
    };

    AudioEngine::AudioEngine()
        : m_impl(std::make_unique<Impl>())
    {
    }

    // Impl is complete here, so unique_ptr can destroy it.
    AudioEngine::~AudioEngine() = default;

    SoundHandle AudioEngine::LoadSound(const wchar_t* /*path*/, SoundCategory /*category*/)
    {
        return kInvalidSound;
    }

    void AudioEngine::Play(SoundHandle /*sound*/, float /*volume*/) {}

    void AudioEngine::SetCategoryVolume(SoundCategory /*category*/, float /*volume*/) {}

    void AudioEngine::SetMasterVolume(float /*volume*/) {}

    void AudioEngine::Update() {}
}
