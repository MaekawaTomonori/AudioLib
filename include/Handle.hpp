#ifndef AUDIO_HPP_
#define AUDIO_HPP_

#include <cstdint>
#include <string_view>

#include "PlaybackHandle.hpp"

namespace Audio {

    class Handle {
        uint64_t handle_ = 0;

    public:
        /** Load Handle
         * @param _path The file path to the audio file
         */
        explicit Handle(std::string_view _path);

        PlaybackHandle Play(const float& _volume = 1.f, const float& _pitch = 1.f, const bool& _loop = false) const;

        //void StopAll();
    };

} // namespace Audio

#endif // AUDIO_HPP_
