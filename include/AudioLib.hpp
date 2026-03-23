#ifndef AudioLib_HPP_
#define AudioLib_HPP_
#include "Handle.hpp"
#include "TrackHandle.hpp"

namespace Audio {
    void Initialize();
    void Shutdown();

    void SetMasterVolume(float _volume);

    TrackHandle CreateTrack();
    void        DestroyTrack(const TrackHandle& _track);
}; // namespace Audio

#endif // AudioLib_HPP_
