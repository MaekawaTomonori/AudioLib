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

        void Stop() const;
        void Pause() const;
        void Resume() const;

        void SetVolume(const float& _volume) const;
        void SetPan(const float& _pan) const;
        void SetPitch(const float& _pitch) const;
        void SetLoop(const bool& _loop) const;
        void Mute(const bool& _mute) const;
    };

} // namespace Audio

#endif // PlaybackHandle_HPP_
