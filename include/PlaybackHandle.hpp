#ifndef PlaybackHandle_HPP_
#define PlaybackHandle_HPP_

#include <cstdint>

namespace Audio {

    class PlaybackHandle {
        uint64_t id_ = 0;

    public:
        PlaybackHandle() = default;
        explicit PlaybackHandle(uint64_t _id) : id_(_id) {}

        bool IsValid() const { return id_ != 0; }
        bool IsPlaying() const;

        void Stop();
        void Pause();
        void Resume();

        void SetVolume(float _volume);
        void SetPan(float _pan);
        void SetPitch(float _pitch);
        void Mute(bool _mute);
    };

} // namespace Audio

#endif // PlaybackHandle_HPP_
