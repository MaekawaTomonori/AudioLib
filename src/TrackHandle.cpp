#include "include/TrackHandle.hpp"

#include "src/Internal.hpp"

namespace Audio {

    void TrackHandle::SetVolume(float _volume) const {
        if (!IsValid()) return;
        Internal::GetMixer()->SetTrackVolume(id_, _volume);
    }

    void TrackHandle::SetPan(float _pan) const {
        if (!IsValid()) return;
        Internal::GetMixer()->SetTrackPan(id_, _pan);
    }

    void TrackHandle::SetPitch(float _pitch) const {
        if (!IsValid()) return;
        Internal::GetMixer()->SetTrackPitch(id_, _pitch);
    }

    void TrackHandle::StopAll() const {
        if (!IsValid()) return;
        Internal::GetMixer()->StopAllInTrack(id_);
    }

} // namespace Audio
