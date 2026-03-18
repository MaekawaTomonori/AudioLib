#include "include/PlaybackHandle.hpp"

#include "src/Internal.hpp"

namespace Audio {

    bool PlaybackHandle::IsPlaying() const {
        if (!IsValid()) return false;
        return Internal::GetMixer()->IsPlaying(id_);
    }

    void PlaybackHandle::Stop() const {
        if (!IsValid()) return;
        Internal::GetMixer()->Stop(id_);
    }

    void PlaybackHandle::Pause() const {
        if (!IsValid()) return;
        Internal::GetMixer()->Pause(id_);
    }

    void PlaybackHandle::Resume() const {
        if (!IsValid()) return;
        Internal::GetMixer()->Resume(id_);
    }

    void PlaybackHandle::SetVolume(const float& _volume) const {
        if (!IsValid()) return;
        Internal::GetMixer()->SetVolume(id_, _volume);
    }

    void PlaybackHandle::SetPan(const float& _pan) const {
        if (!IsValid()) return;
        Internal::GetMixer()->SetPan(id_, _pan);
    }

    void PlaybackHandle::SetPitch(const float& _pitch) const {
        if (!IsValid()) return;
        Internal::GetMixer()->SetPitch(id_, _pitch);
    }

    void PlaybackHandle::SetLoop(const bool& _loop) const {
        if (!IsValid()) return;
        Internal::GetMixer()->SetLoop(id_, _loop);
    }

    void PlaybackHandle::Mute(const bool& _mute) const {
        if (!IsValid()) return;
        Internal::GetMixer()->Mute(id_, _mute);
    }

} // namespace Audio
