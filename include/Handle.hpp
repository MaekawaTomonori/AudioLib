#ifndef AUDIO_HPP_
#define AUDIO_HPP_

#include <cstdint>
#include <string_view>

#include "PlaybackHandle.hpp"
#include "TrackHandle.hpp"

namespace Audio {

    class Handle {
        uint64_t handle_ = 0;

    public:
        /** Load Handle
         * @param _path The file path to the audio file
         */
        explicit Handle(std::string_view _path);

        /** Play Audio 
         * @param _volume The volume of the audio (0.0 to 1.0)
         * @param _pitch The pitch of the audio (0.5 to 2.0)
         * @param _pan The pan of the audio (-1.0 to 1.0)
         * @param _loop Whether the audio should loop
         * @param _track The track handle to play the audio on
         */
        PlaybackHandle Play(const float&       _volume = 1.f,
                            const float&       _pitch  = 1.f,
                            const float&       _pan    = 0.f,
                            const bool&        _loop   = false,
                            const TrackHandle& _track  = {}) const;

        /** Stop All Audio
         * Stops all audio associated with this handle 
         */
        void StopAll() const;
    };

} // namespace Audio

#endif // AUDIO_HPP_
