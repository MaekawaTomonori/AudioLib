#include "Mixer.hpp"

#include <memory>
#include <ranges>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

namespace Audio::Internal {

    struct Mixer::PlaybackInstance {
        enum class State { Playing, Paused, Stopped };

        uint64_t            soundId  = 0;
        uint64_t            trackId  = 0;
        ma_audio_buffer_ref bufferRef{};
        ma_sound*           sound    = nullptr;
        float               volume   = 1.0f;
        bool                muted    = false;
        State               state    = State::Playing;

        ~PlaybackInstance() {
            if (sound) {
                ma_sound_uninit(sound);
                delete sound;
            }
            ma_audio_buffer_ref_uninit(&bufferRef);
        }
    };

    struct Mixer::TrackInstance {
        ma_sound_group* group = nullptr;

        ~TrackInstance() {
            if (group) {
                ma_sound_group_uninit(group);
                delete group;
            }
        }
    };


    Mixer::~Mixer() {
        Shutdown();
    }

    bool Mixer::Init() {
        if (engine_) return true;
        engine_ = new ma_engine();
        if (ma_engine_init(nullptr, engine_) != MA_SUCCESS) {
            delete engine_;
            engine_ = nullptr;
            return false;
        }
        return true;
    }

    void Mixer::Shutdown() {
        if (!engine_) return;

        sounds_.clear(); // sounds must be uninit before their groups
        tracks_.clear(); // TrackInstance destructors handle ma_sound_group cleanup

        ma_engine_uninit(engine_);
        delete engine_;
        engine_ = nullptr;
    }

    uint64_t Mixer::Play(uint64_t _soundId, const Sound& _sound, uint64_t _trackId) {
        if (!engine_) return 0;
        if (!_sound.IsValid()) return 0;

        ma_sound_group* group = nullptr;
        if (_trackId != 0) {
            const auto it = tracks_.find(_trackId);
            if (it != tracks_.end()) group = it->second->group;
        }

        // Extract a stopped instance of the same sound for reuse (before cleanup)
        std::shared_ptr<PlaybackInstance> reused;
        for (auto it = sounds_.begin(); it != sounds_.end(); ++it) {
            if (it->second->soundId == _soundId && it->second->state == PlaybackInstance::State::Stopped) {
                reused = std::move(it->second);
                sounds_.erase(it);
                break;
            }
        }

        // Remove all remaining stopped instances
        Cleanup();

        // Restart the reused instance if available
        if (reused) {
            ma_sound_seek_to_pcm_frame(reused->sound, 0);
            ma_audio_buffer_ref_seek_to_pcm_frame(&reused->bufferRef, 0);
            if (ma_sound_start(reused->sound) == MA_SUCCESS) {
                reused->state = PlaybackInstance::State::Playing;
                const uint64_t id = nextId_++;
                sounds_[id] = std::move(reused);
                return id;
            }
        }

        // Allocate a new instance
        auto instance = std::make_shared<PlaybackInstance>();
        instance->soundId = _soundId;
        instance->trackId = _trackId;

        if (ma_audio_buffer_ref_init(
                ma_format_f32,
                _sound.GetChannels(),
                _sound.GetPcm().data(),
                _sound.GetFrameCount(),
                &instance->bufferRef) != MA_SUCCESS) {
            return 0;
        }

        instance->sound = new ma_sound();
        if (ma_sound_init_from_data_source(engine_, &instance->bufferRef, 0, group, instance->sound) != MA_SUCCESS
            || ma_sound_start(instance->sound) != MA_SUCCESS) {
            return 0; // destructor cleans up on shared_ptr release
        }

        const uint64_t id = nextId_++;
        sounds_[id] = std::move(instance);
        return id;
    }

    void Mixer::Cleanup() {
        for (auto it = sounds_.begin(); it != sounds_.end(); ) {
            if (it->second->state == PlaybackInstance::State::Stopped) {
                it = sounds_.erase(it); // shared_ptr destructor handles resource cleanup
            } else {
                ++it;
            }
        }
    }

    bool Mixer::IsPlaying(uint64_t _id) const {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return false;
        return ma_sound_is_playing(it->second->sound) == MA_TRUE;
    }

    void Mixer::Stop(uint64_t _id) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_stop(it->second->sound);
        ma_sound_seek_to_pcm_frame(it->second->sound, 0);
        ma_audio_buffer_ref_seek_to_pcm_frame(&it->second->bufferRef, 0); // keep position in sync for potential reuse
        it->second->state = PlaybackInstance::State::Stopped;
    }

    void Mixer::StopAll(uint64_t _soundId) {
        for (auto& instance : sounds_ | std::views::values) {
            if (instance->soundId == _soundId) {
                ma_sound_stop(instance->sound);
                ma_sound_seek_to_pcm_frame(instance->sound, 0);
                ma_audio_buffer_ref_seek_to_pcm_frame(&instance->bufferRef, 0);
                instance->state = PlaybackInstance::State::Stopped;
            }
        }
    }

    void Mixer::Pause(uint64_t _id) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_stop(it->second->sound);
        it->second->state = PlaybackInstance::State::Paused;
    }

    void Mixer::Resume(uint64_t _id) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        if (it->second->state != PlaybackInstance::State::Paused) return;
        ma_sound_start(it->second->sound);
        it->second->state = PlaybackInstance::State::Playing;
    }

    void Mixer::SetVolume(uint64_t _id, float _volume) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        PlaybackInstance& instance = *it->second;
        instance.volume = _volume;
        if (!instance.muted) ma_sound_set_volume(instance.sound, _volume);
    }

    void Mixer::SetPan(uint64_t _id, float _pan) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_set_pan(it->second->sound, _pan);
    }

    void Mixer::SetPitch(uint64_t _id, float _pitch) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_set_pitch(it->second->sound, _pitch);
    }

    void Mixer::SetLoop(uint64_t _id, bool _loop) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_set_looping(it->second->sound, _loop ? MA_TRUE : MA_FALSE);
    }

    void Mixer::Mute(uint64_t _id, bool _mute) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        PlaybackInstance& instance = *it->second;
        instance.muted = _mute;
        ma_sound_set_volume(instance.sound, _mute ? 0.0f : instance.volume);
    }

    uint64_t Mixer::CreateTrack() {
        if (!engine_) return 0;
        auto track = std::make_shared<TrackInstance>();
        track->group = new ma_sound_group();
        if (ma_sound_group_init(engine_, 0, nullptr, track->group) != MA_SUCCESS) {
            return 0;
        }
        const uint64_t id = nextTrackId_++;
        tracks_[id] = std::move(track);
        return id;
    }

    void Mixer::DestroyTrack(uint64_t _trackId) {
        // Remove all playback instances belonging to this track before destroying the group.
        // This ensures ma_sound is uninit before ma_sound_group_uninit (called by TrackInstance dtor).
        for (auto it = sounds_.begin(); it != sounds_.end(); ) {
            if (it->second->trackId == _trackId) {
                it = sounds_.erase(it);
            } else {
                ++it;
            }
        }
        tracks_.erase(_trackId);
    }

    void Mixer::StopAllInTrack(uint64_t _trackId) {
        for (auto& instance : sounds_ | std::views::values) {
            if (instance->trackId == _trackId) {
                ma_sound_stop(instance->sound);
                ma_sound_seek_to_pcm_frame(instance->sound, 0);
                ma_audio_buffer_ref_seek_to_pcm_frame(&instance->bufferRef, 0);
                instance->state = PlaybackInstance::State::Stopped;
            }
        }
    }

    void Mixer::SetTrackVolume(uint64_t _trackId, float _volume) {
        const auto it = tracks_.find(_trackId);
        if (it == tracks_.end()) return;
        ma_sound_group_set_volume(it->second->group, _volume);
    }

    void Mixer::SetTrackPan(uint64_t _trackId, float _pan) {
        const auto it = tracks_.find(_trackId);
        if (it == tracks_.end()) return;
        ma_sound_group_set_pan(it->second->group, _pan);
    }

    void Mixer::SetTrackPitch(uint64_t _trackId, float _pitch) {
        const auto it = tracks_.find(_trackId);
        if (it == tracks_.end()) return;
        ma_sound_group_set_pitch(it->second->group, _pitch);
    }

    void Mixer::SetMasterVolume(float _volume) const {
        if (!engine_) return;
        ma_engine_set_volume(engine_, _volume);
    }

} // namespace Audio::Internal
