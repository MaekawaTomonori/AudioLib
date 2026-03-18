#ifndef Mixer_HPP_
#define Mixer_HPP_

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Sound.hpp"

struct ma_engine;

namespace Audio {

    class Mixer {
        struct PlaybackInstance;

        ma_engine* engine_ = nullptr;
        std::unordered_map<uint64_t, std::shared_ptr<PlaybackInstance>> sounds_;
        uint64_t nextId_ = 1;

    public:
        ~Mixer();
        bool Init();
        void Shutdown();

        uint64_t Play(uint64_t _soundId, const Sound& _sound);
        bool IsPlaying(uint64_t _id) const;

        void Stop(uint64_t _id);
        void StopAll(uint64_t _soundId);
        void Pause(uint64_t _id);
        void Resume(uint64_t _id);
        void Mute(uint64_t _id, bool _mute);
        void SetVolume(uint64_t _id, float _volume);
        void SetPan(uint64_t _id, float _pan);
        void SetPitch(uint64_t _id, float _pitch);
        void SetLoop(uint64_t _id, bool _loop);
        void SetMasterVolume(float _volume) const;

    private:
        void Cleanup();
    };

} // namespace Audio

#endif // Mixer_HPP_
