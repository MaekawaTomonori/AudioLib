#ifndef TrackHandle_HPP_
#define TrackHandle_HPP_

#include <cstdint>

namespace Audio {

    class TrackHandle {
        uint64_t id_ = 0;

    public:
        TrackHandle() = default;
        explicit TrackHandle(uint64_t _id) : id_(_id) {}

        bool     IsValid() const { return id_ != 0; }
        uint64_t GetId()   const { return id_; }

        void SetVolume(float _volume) const;
        void SetPan(float _pan)       const;
        void SetPitch(float _pitch)   const;
        void StopAll()                const;
    };

} // namespace Audio

#endif // TrackHandle_HPP_
