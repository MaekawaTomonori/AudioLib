#ifndef AUDIO_HPP_
#define AUDIO_HPP_

#include <cstdint>
#include <string_view>

#include "PlaybackHandle.hpp"
#include "Result.hpp"
#include "TrackHandle.hpp"

namespace Audio {

    class Handle {
        uint64_t handle_       = 0;
        Result   latestResult_;

    public:
        Handle() = default;

        /** Load Handle (fails silently — Play becomes a no-op on failure).
         *  Use IsValid() to check the outcome after construction.
         * @param _path The file path to the audio file
         */
        explicit Handle(const std::string& _path);

        /** Load audio and return the result.
         *  Also stores the result internally — accessible via IsValid().
         *  This function does not need to be called if result checking is not required.
         * @param _path The file path to the audio file
         */
        Result Load(const std::string& _path);

        /** Returns the result of the most recent load operation.
         *  Evaluates to true if the handle is valid: if (handle.IsValid()) { ... }
         *  On failure: handle.IsValid().what() returns the error description.
         */
        const Result& IsValid() const { return latestResult_; }

        /** Play Audio
         * @param _volume The volume of the audio (0.0 to 1.0)
         * @param _pitch The pitch of the audio (0.5 to 2.0)
         * @param _pan The pan of the audio (-1.0 to 1.0)
         * @param _loop Whether the audio should loop
         * @param _track The track handle to play the audio on
         */
        PlaybackHandle Play(float        _volume = 1.f,
                            float        _pitch  = 1.f,
                            float        _pan    = 0.f,
                            bool         _loop   = false,
                            const TrackHandle& _track  = {}) const;

        /** Stop All Audio
         * Stops all audio associated with this handle
         */
        void StopAll() const;
    };

} // namespace Audio

#endif // AUDIO_HPP_
