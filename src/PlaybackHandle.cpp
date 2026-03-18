#include "include/PlaybackHandle.hpp"

#include "src/Internal.hpp"

namespace Audio {

    bool PlaybackHandle::IsPlaying() const {
        if (!IsValid()) return false;
        return Internal::GetMixer()->IsPlaying(id_);
    }

    void PlaybackHandle::Stop()              {}
    void PlaybackHandle::Pause()             {}
    void PlaybackHandle::Resume()            {}

    void PlaybackHandle::SetVolume(float)    {}
    void PlaybackHandle::SetPan(float)       {}
    void PlaybackHandle::SetPitch(float)     {}
    void PlaybackHandle::Mute(bool)          {}

} // namespace Audio
