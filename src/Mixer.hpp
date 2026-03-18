#ifndef Mixer_HPP_
#define Mixer_HPP_

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

struct ma_engine;
struct ma_sound;

namespace Audio {

    class Mixer {
        struct PlaybackInstance {
            ma_sound*   sound = nullptr;
            std::string path;
        };

        ma_engine* engine_ = nullptr;
        std::unordered_map<uint64_t, PlaybackInstance> sounds_;
        uint64_t nextId_ = 1;

    public:
        ~Mixer();
        bool Init();
        void Shutdown();

        uint64_t Play(std::string_view _path);
        bool IsPlaying(uint64_t _id) const;

        void Stop(uint64_t _id);
        void Pause(uint64_t _id);
        void Resume(uint64_t _id);
        void Mute(uint64_t _id, bool _mute);
        void SetVolume(uint64_t _id, float _volume);
        void SetPan(uint64_t _id, float _pan);
        void SetPitch(uint64_t _id, float _pitch);
        void SetMasterVolume(float _volume);
        void Mix(float* _output, uint32_t _frameCount, uint32_t _channels);
    };

} // namespace Audio

#endif // Mixer_HPP_
