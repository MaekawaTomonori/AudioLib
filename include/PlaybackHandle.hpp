#ifndef PlaybackHandle_HPP_
#define PlaybackHandle_HPP_

#include <cstdint>

namespace Audio {

    class PlaybackHandle {
        uint64_t id_ = 0;

    public:
        PlaybackHandle() = default;
        explicit PlaybackHandle(uint64_t _id) : id_(_id) {}

        /** Returns whether this handle refers to a valid playback instance */
        bool IsValid() const { return id_ != 0; }

        /** Returns whether the audio is currently playing */
        bool IsPlaying() const;

        /** Stop playback and reset position to the beginning */
        void Stop() const;

        /** Pause playback, preserving the current position */
        void Pause() const;

        /** Resume a paused playback from where it was paused */
        void Resume() const;

        /** Set the playback volume
         * @param _volume Volume level (0.0 = silent, 1.0 = full)
         */
        void SetVolume(const float& _volume) const;

        /** Set the stereo pan position
         * @param _pan Pan position (-1.0 = full left, 0.0 = center, 1.0 = full right)
         */
        void SetPan(const float& _pan) const;

        /** Set the playback pitch
         * @param _pitch Pitch multiplier (1.0 = original, 2.0 = one octave up)
         */
        void SetPitch(const float& _pitch) const;

        /** Enable or disable looping
         * @param _loop true to loop indefinitely, false to play once
         */
        void SetLoop(const bool& _loop) const;

        /** Mute or unmute this playback without changing the stored volume
         * @param _mute true to mute, false to restore volume
         */
        void Mute(const bool& _mute) const;
    };

} // namespace Audio

#endif // PlaybackHandle_HPP_
