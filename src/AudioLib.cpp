#include "include/AudioLib.hpp"
#include "include/TrackHandle.hpp"
#include "src/Internal.hpp"

namespace Audio {

    void Initialize() {
        Internal::GetMixer()->Init();
    }

    void Shutdown() {
        Internal::GetMixer()->Shutdown();
    }

    void SetMasterVolume(float _volume) {
        Internal::GetMixer()->SetMasterVolume(_volume);
    }

    TrackHandle CreateTrack() {
        return TrackHandle(Internal::GetMixer()->CreateTrack());
    }

    void DestroyTrack(const TrackHandle& _track) {
        Internal::GetMixer()->DestroyTrack(_track.GetId());
    }

} // namespace Audio
