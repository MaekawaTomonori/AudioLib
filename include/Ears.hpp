#ifndef Ears_HPP_
#define Ears_HPP_

namespace Audio {

    /** How one playback reaches each ear: a volume, a delay and a low-pass cutoff for the left and the right channel.
     *  Used for sounds placed around a listener (a sound to the right is louder and earlier in the right ear, and
     *  duller in the left one; a sound behind or through a wall is duller in both).
     *  The default leaves the sound as it is.
     */
    struct Ears {
        /** Volume of each ear (0.0 = silent, 1.0 = unchanged). Values below 0 are taken as 0. */
        float leftVolume  = 1.f;
        float rightVolume = 1.f;
        /** Delay of each ear in seconds (0.0 = none). Held to 0..maxDelay. */
        float leftDelay  = 0.f;
        float rightDelay = 0.f;
        /** Frequency in Hz above which each ear's sound is cut away (12 dB an octave; at the cutoff it is 3 dB lower).
         *  noCutoff or above cuts nothing. Held to 20..noCutoff; NaN is taken as noCutoff. */
        float leftCutoff  = noCutoff;
        float rightCutoff = noCutoff;
        /** How much of the sound goes to the reverb (Audio::SetReverb): 0.0 = none, 1.0 = all of it, before the ears'
         *  own volume, delay and cutoff (the echo comes from all around). Held to 0..1; NaN is taken as 0. */
        float send = 0.f;

        /** The longest delay an ear can have, in seconds (1 ms; a head is about 0.66 ms wide). */
        static constexpr float maxDelay = 0.001f;
        /** A cutoff that cuts nothing (the top of hearing). */
        static constexpr float noCutoff = 20000.f;
    };

} // namespace Audio

#endif // Ears_HPP_
