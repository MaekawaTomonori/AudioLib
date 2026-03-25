#ifndef TrackHandle_HPP_
#define TrackHandle_HPP_

#include <cstdint>

namespace Audio {

    class TrackHandle {
        uint64_t id_ = 0;

    public:
        TrackHandle() = default;
        explicit TrackHandle(uint64_t _id) : id_(_id) {}

        /** Returns whether this handle refers to a valid track */
        bool     IsValid() const { return id_ != 0; }

        /** Returns the internal track ID */
        uint64_t GetId()   const { return id_; }

        /** Set the track volume, applied to all playbacks routed through this track
         * @param _volume Volume level (0.0 = silent, 1.0 = full)
         */
        void SetVolume(float _volume) const;

        /** Set the stereo pan position for the entire track
         * @param _pan Pan position (-1.0 = full left, 0.0 = center, 1.0 = full right)
         */
        void SetPan(float _pan) const;

        /** Set the pitch for the entire track
         * @param _pitch Pitch multiplier (1.0 = original, 2.0 = one octave up)
         */
        void SetPitch(float _pitch) const;

        /** Stop all playbacks currently routed through this track */
        void StopAll() const;
    };

} // namespace Audio

#endif // TrackHandle_HPP_
