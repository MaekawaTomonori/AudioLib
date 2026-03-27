#include "include/AudioLib.hpp"

#include <mutex>

#include "include/TrackHandle.hpp"
#include "src/Internal.hpp"

namespace {
    std::once_flag flag;
}

namespace Audio {

    bool Initialize() {
        bool result = true;
        std::call_once(flag, [&result] { result = Internal::GetMixer()->Init(); });
        return result;
    }

    void Shutdown() {
        Internal::GetMixer()->Shutdown();
        Internal::GetRepository()->Clear();
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
