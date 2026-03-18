#include "Mixer.hpp"

#include <string>

#include <ranges>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

namespace Audio {

    Mixer::~Mixer() {
        Shutdown();
    }

    bool Mixer::Init() {
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

        for (auto& instance : sounds_ | std::views::values) {
            ma_sound_uninit(instance.sound);
            delete instance.sound;
        }
        sounds_.clear();

        ma_engine_uninit(engine_);
        delete engine_;
        engine_ = nullptr;
    }

    uint64_t Mixer::Play(std::string_view _path) {
        if (!engine_) return 0;

        for (auto it = sounds_.begin(); it != sounds_.end(); ++it) {
            PlaybackInstance& instance = it->second;
            if (instance.path == _path && !ma_sound_is_playing(instance.sound)) {
                ma_sound_seek_to_pcm_frame(instance.sound, 0);
                if (ma_sound_start(instance.sound) != MA_SUCCESS) break;
                ma_sound* reused = instance.sound;
                sounds_.erase(it);
                const uint64_t id = nextId_++;
                sounds_[id] = { reused, std::string(_path) };
                return id;
            }
        }

        auto* sound = new ma_sound();
        if (ma_sound_init_from_file(engine_, std::string(_path).c_str(), 0, nullptr, nullptr, sound) != MA_SUCCESS) {
            delete sound;
            return 0;
        }
        if (ma_sound_start(sound) != MA_SUCCESS) {
            ma_sound_uninit(sound);
            delete sound;
            return 0;
        }

        const uint64_t id = nextId_++;
        sounds_[id] = { sound, std::string(_path) };
        return id;
    }

    bool Mixer::IsPlaying(uint64_t _id) const {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return false;
        return ma_sound_is_playing(it->second.sound) == MA_TRUE;
    }

    void Mixer::Stop(uint64_t _id) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_stop(it->second.sound);
        ma_sound_seek_to_pcm_frame(it->second.sound, 0);
    }

    void Mixer::Pause(uint64_t _id) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_stop(it->second.sound);
    }

    void Mixer::Resume(uint64_t _id) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_start(it->second.sound);
    }

    void Mixer::SetVolume(uint64_t _id, float _volume) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        PlaybackInstance& instance = it->second;
        instance.volume = _volume;
        if (!instance.muted) ma_sound_set_volume(instance.sound, _volume);
    }

    void Mixer::SetPan(uint64_t _id, float _pan) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_set_pan(it->second.sound, _pan);
    }

    void Mixer::SetPitch(uint64_t _id, float _pitch) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_set_pitch(it->second.sound, _pitch);
    }

    void Mixer::SetLoop(uint64_t _id, bool _loop) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        ma_sound_set_looping(it->second.sound, _loop ? MA_TRUE : MA_FALSE);
    }

    void Mixer::Mute(uint64_t _id, bool _mute) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        PlaybackInstance& instance = it->second;
        instance.muted = _mute;
        ma_sound_set_volume(instance.sound, _mute ? 0.0f : instance.volume);
    }

    void Mixer::SetMasterVolume(float _volume) const {
        if (!engine_) return;
        ma_engine_set_volume(engine_, _volume);
    }

} // namespace Audio
