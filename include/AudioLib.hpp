#ifndef AudioLib_HPP_
#define AudioLib_HPP_
#include "Handle.hpp"
#include "Reverb.hpp"
#include "TrackHandle.hpp"

namespace Audio {

    /** Initialize the audio system. Must be called before any other Audio function.
     * @return true on success, false if the audio device could not be opened
     */
    bool Initialize();

    /** Shut down the audio system and release all audio resources including loaded sounds */
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

    /** Set the one reverb that playbacks send to with Ears::send (the echo of the listener's place).
     *  Values move smoothly from the current ones. Nothing is computed while its volume is 0 (the default) or once the
     *  echo has died away. Does nothing before Initialize or when the output is not stereo.
     * @param _reverb The decay time, damping, predelay and volume of the reverb
     */
    void SetReverb(const Reverb& _reverb);

    /** Set one of the reverbs that playbacks send to with Ears::send and Ears::reverb (the echo of a place; reverb 0 is the
     *  one SetReverb(reverb) sets). Otherwise as SetReverb above. Does nothing for an index outside 0..Reverb::count-1.
     * @param _index Which reverb (0..Reverb::count-1)
     * @param _reverb The decay time, damping, predelay and volume of the reverb
     */
    void SetReverb(int _index, const Reverb& _reverb);

} // namespace Audio

#endif // AudioLib_HPP_
