#ifndef Reverb_HPP_
#define Reverb_HPP_

namespace Audio {

    /** A reverb playbacks can send to (Ears::send, to the one Ears::reverb names): the echo of a place. There are
     *  Reverb::count of them; reverb 0 is the one Audio::SetReverb(reverb) sets (the echo of the place the listener is in),
     *  the others are set with Audio::SetReverb(index, reverb) (the echo of the places other sounds are in).
     *  Each one's volume is 0 until set, and while it is 0 nothing is computed for it.
     */
    struct Reverb {
        /** Seconds for the echo to fall by 60 dB (RT60): longer in big, hard places. Held to 0.1..20. */
        float decay = 1.5f;
        /** How much faster the high sounds die than the low ones (0.0 = the same, 1.0 = much faster: soft walls). Held to 0..1. */
        float damping = 0.5f;
        /** Seconds before the echo begins (the way to the nearest wall and back). Held to 0..maxPredelay. */
        float predelay = 0.f;
        /** Volume of the echo (0.0 = off). Values below 0 are taken as 0. */
        float volume = 0.f;

        /** The longest predelay, in seconds. */
        static constexpr float maxPredelay = 0.1f;
        /** How many reverbs there are (indices 0..count-1). */
        static constexpr int count = 4;
    };

} // namespace Audio

#endif // Reverb_HPP_
