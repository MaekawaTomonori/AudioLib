#ifndef AudioLib_HPP_
#define AudioLib_HPP_
#include "Handle.hpp"

namespace Audio {
    void Initialize();
    void Shutdown();

    void SetMasterVolume(float _volume);
}; // namespace Audio

#endif // AudioLib_HPP_
