#include "include/AudioLib.hpp"
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

} // namespace Audio
