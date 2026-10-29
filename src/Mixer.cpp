#include "Mixer.hpp"

#include <memory>
#include <ranges>

#include "Ear.hpp"
#include "vendor/miniaudio.h"

namespace Audio::Internal {

    struct Mixer::PlaybackInstance {
        enum class State { Playing, Paused, Stopped };

        uint64_t            soundId  = 0;
        uint64_t            trackId  = 0;
        std::shared_ptr<const Sound> asset;
        ma_audio_buffer_ref bufferRef{};
        // ストリーミング再生のときだけ。データソースは bufferRef ではなくこれ
        std::shared_ptr<StreamingVoice> stream;
        ma_sound*           sound    = nullptr;
        // Ears を使う再生だけ（AttachEar）。ma_sound の後に外す（デストラクタの本体で ma_sound を外し、その後でメンバとして破棄される）
        std::unique_ptr<Ear> ear;
        float               volume   = 1.0f;
        bool                muted    = false;
        State               state    = State::Playing;

        // ma_sound を先に外す。その後でメンバの stream が手放されるので、オーディオスレッドはもう読んでいない
        ~PlaybackInstance() {
            if (sound) {
                ma_sound_uninit(sound);
                delete sound;
            }
            if (!stream) ma_audio_buffer_ref_uninit(&bufferRef);
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
        refiller_.Shutdown(); // 再生を外した後に止める。残ったデコーダはここで破棄される
        tracks_.clear(); // TrackInstance destructors handle ma_sound_group cleanup

        ma_engine_uninit(engine_);
        delete engine_;
        engine_ = nullptr;
    }

    uint64_t Mixer::Play(uint64_t _soundId, std::shared_ptr<const Sound> _sound, uint64_t _trackId, const Ears* _ears) {
        if (!engine_) return 0;
        if (!_sound || !_sound->IsValid()) return 0;

        // Extract a stopped instance of the same sound for reuse (before cleanup)
        std::shared_ptr<PlaybackInstance> reused;
        for (auto it = sounds_.begin(); it != sounds_.end(); ++it) {
            if (it->second->soundId == _soundId && !it->second->stream && it->second->state == PlaybackInstance::State::Stopped) {
                reused = std::move(it->second);
                sounds_.erase(it);
                break;
            }
        }

        // Remove all remaining stopped instances
        Cleanup();

        // Restart the reused instance if available
        if (reused) {
            reused->asset = _sound;
            ma_sound_seek_to_pcm_frame(reused->sound, 0);
            ma_audio_buffer_ref_seek_to_pcm_frame(&reused->bufferRef, 0);
            PrepareEar(*reused, _ears);
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
        instance->asset = std::move(_sound);

        if (ma_audio_buffer_ref_init(
                ma_format_f32,
                instance->asset->GetChannels(),
                instance->asset->GetPcm().data(),
                instance->asset->GetFrameCount(),
                &instance->bufferRef) != MA_SUCCESS) {
            return 0;
        }

        void* source = &instance->bufferRef;
        return StartInstance(std::move(instance), source, _trackId, _ears);
    }

    uint64_t Mixer::PlayStreaming(uint64_t _soundId, std::shared_ptr<const StreamingAsset> _asset, uint64_t _trackId, const Ears* _ears) {
        if (!engine_) return 0;
        if (!_asset || !_asset->IsValid()) return 0;

        Cleanup();

        auto instance = std::make_shared<PlaybackInstance>();
        instance->soundId = _soundId;
        instance->trackId = _trackId;
        instance->stream  = std::make_shared<StreamingVoice>(std::move(_asset));

        void* source = instance->stream->DataSource();
        auto  voice  = instance->stream;
        const uint64_t id = StartInstance(std::move(instance), source, _trackId, _ears);
        // 準備（デコーダを開いて 0.5 秒分）はワーカーがする。それまでデータソースは無音を返す
        if (id != 0) refiller_.Add(voice);
        return id;
    }

    uint64_t Mixer::StartInstance(std::shared_ptr<PlaybackInstance> _instance, void* _dataSource, uint64_t _trackId, const Ears* _ears) {
        ma_sound_group* group = nullptr;
        if (_trackId != 0) {
            const auto it = tracks_.find(_trackId);
            if (it != tracks_.end()) group = it->second->group;
        }

        _instance->sound = new ma_sound();
        if (ma_sound_init_from_data_source(engine_, _dataSource, 0, group, _instance->sound) != MA_SUCCESS) {
            delete _instance->sound;
            _instance->sound = nullptr; // destructor の ma_sound_uninit 呼び出しを防ぐ
            return 0;
        }
        PrepareEar(*_instance, _ears);
        if (ma_sound_start(_instance->sound) != MA_SUCCESS) {
            ma_sound_uninit(_instance->sound);
            delete _instance->sound;
            _instance->sound = nullptr;
            return 0;
        }

        const uint64_t id = nextId_++;
        sounds_[id] = std::move(_instance);
        return id;
    }

    void Mixer::PrepareEar(PlaybackInstance& _instance, const Ears* _ears) {
        if (_ears) {
            if (AttachEar(_instance)) _instance.ear->Restart(*_ears);
        } else if (_instance.ear) {
            _instance.ear->Restart({});
        }
    }

    bool Mixer::AttachEar(PlaybackInstance& _instance) {
        if (_instance.ear) return true;
        auto ear = Ear::Create(engine_);
        if (!ear) return false;

        ma_node* destination = ma_engine_get_endpoint(engine_);
        if (_instance.trackId != 0) {
            const auto it = tracks_.find(_instance.trackId);
            if (it != tracks_.end()) destination = it->second->group;
        }
        // 先に Ear から行き先をつなぎ、そのあとで ma_sound の出口を Ear に付け替える（付け替えは前のつながりを外す）。
        // 鳴っている途中に付け替えると、その間の 1 回の処理（10 ms ほど）が欠けることがある。鳴らす前（Play に Ears を渡す）なら欠けない
        auto* node = static_cast<ma_node*>(ear->AsNode());
        if (ma_node_attach_output_bus(node, 0, destination, 0) != MA_SUCCESS) return false;
        if (ma_node_attach_output_bus(_instance.sound, 0, node, 0) != MA_SUCCESS) {
            ma_node_attach_output_bus(_instance.sound, 0, destination, 0);
            return false;
        }
        _instance.ear = std::move(ear);
        return true;
    }

    void Mixer::StopInstance(PlaybackInstance& _instance) {
        ma_sound_stop(_instance.sound);
        if (!_instance.stream) {
            ma_sound_seek_to_pcm_frame(_instance.sound, 0);
            ma_audio_buffer_ref_seek_to_pcm_frame(&_instance.bufferRef, 0); // keep position in sync for potential reuse
        }
        _instance.state = PlaybackInstance::State::Stopped;
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
        StopInstance(*it->second);
    }

    void Mixer::StopAll(uint64_t _soundId) {
        for (auto& instance : sounds_ | std::views::values) {
            if (instance->soundId == _soundId) StopInstance(*instance);
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

    void Mixer::SetEars(uint64_t _id, const Ears& _ears) {
        const auto it = sounds_.find(_id);
        if (it == sounds_.end()) return;
        auto& instance = *it->second;
        if (instance.ear) {
            instance.ear->Set(_ears);
        } else if (AttachEar(instance)) {
            instance.ear->Restart(_ears);
        }
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
            if (instance->trackId == _trackId) StopInstance(*instance);
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
