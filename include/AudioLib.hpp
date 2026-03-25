#ifndef AudioLib_HPP_
#define AudioLib_HPP_
#include "Handle.hpp"
#include "TrackHandle.hpp"

namespace Audio {

    /** Initialize the audio system. Must be called before any other Audio function */
    void Initialize();

    /** Shut down the audio system and release all audio resources */
    void Shutdown();

    /** Set the master output volume, applied to all tracks and playbacks
     * @param _volume Volume level (0.0 = silent, 1.0 = full)
     */
    void SetMasterVolume(float _volume);

    /** Create a new mixing track
     * @return A TrackHandle identifying the created track. Returns an invalid handle on failure
     */
    TrackHandle CreateTrack();

    /** Destroy a track and stop all playbacks routed through it
     * @param _track The track to destroy
     */
    void DestroyTrack(const TrackHandle& _track);

} // namespace Audio

#endif // AudioLib_HPP_
