#ifndef Ears_HPP_
#define Ears_HPP_

namespace Audio {

    /** How one playback reaches each ear: a volume and a delay for the left and the right channel.
     *  Used for sounds placed around a listener (a sound to the right is louder and earlier in the right ear).
     *  The default leaves the sound as it is.
     */
    struct Ears {
        /** Volume of each ear (0.0 = silent, 1.0 = unchanged). Values below 0 are taken as 0. */
        float leftVolume  = 1.f;
        float rightVolume = 1.f;
        /** Delay of each ear in seconds (0.0 = none). Held to 0..maxDelay. */
        float leftDelay  = 0.f;
        float rightDelay = 0.f;

        /** The longest delay an ear can have, in seconds (1 ms; a head is about 0.66 ms wide). */
        static constexpr float maxDelay = 0.001f;
    };

} // namespace Audio

#endif // Ears_HPP_
