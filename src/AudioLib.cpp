#include "include/AudioLib.hpp"
#include "src/Internal.hpp"

namespace Audio {

    void Initialize() {
        Internal::GetMixer()->Init();
    }

    void Shutdown() {
        Internal::GetMixer()->Shutdown();
    }

} // namespace Audio
