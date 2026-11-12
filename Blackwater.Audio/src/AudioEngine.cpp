#include "Blackwater/Audio/AudioEngine.h"

//
// Stub implementation: the API compiles and links, nothing makes sound yet.
// The XAudio2 backend replaces these bodies -- see docs/Audio.md.
//

namespace bw
{
    struct AudioEngine::Impl
    {
        // XAudio2 device, mastering voice, one submix voice per category,
        // loaded sounds, voice pool.
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
