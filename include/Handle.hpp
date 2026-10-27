#ifndef AUDIO_HPP_
#define AUDIO_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "PlaybackHandle.hpp"
#include "Result.hpp"
#include "TrackHandle.hpp"

namespace Audio {

    class Handle {
        struct Impl;
        std::shared_ptr<Impl> impl_;

    public:
        Handle();
        ~Handle() = default;

        Handle(Handle&&) noexcept            = default;
        Handle& operator=(Handle&&) noexcept = default;
        Handle(const Handle&)                = delete;
        Handle& operator=(const Handle&)     = delete;

        /** Load Handle (fails silently — Play becomes a no-op on failure).
         *  Use IsValid() to check the outcome after construction.
         * @param _path The file path to the audio file
         */
        explicit Handle(const std::string& _path);

        /** Submit an async load. Returns immediately.
         *  Call IsValid() to wait for completion and check the outcome.
         * @param _path The file path to the audio file
         */
        void Load(const std::string& _path) const;

        /** Submit an async load of encoded audio held in memory (WAV, MP3, FLAC or OGG Vorbis). Returns immediately.
         *  If a sound is already loaded under _name, it is reused and _bytes is not decoded.
         *  Call IsValid() to wait for completion and check the outcome.
         * @param _name The name that identifies the sound (shares names with file paths loaded by Load(path))
         * @param _bytes The encoded audio data
         */
        void Load(const std::string& _name, std::vector<uint8_t> _bytes) const;

        /** Non-blocking: returns true once loading has finished (Loaded or Failed).
         *  Returns true for a default-constructed (never loaded) Handle as well.
         */
        bool IsReady() const;

        /** Wait until loading is complete, then return the result.
         *  Evaluates to true if the handle is valid: if (handle.IsValid()) { ... }
         *  On failure: handle.IsValid().what() returns the error description.
         */
        const Result& IsValid() const;

        /** Play Audio. Blocks until loading is complete if called before load finishes.
         * @param _volume The volume of the audio (0.0 to 1.0)
         * @param _pitch The pitch of the audio (0.5 to 2.0)
         * @param _pan The pan of the audio (-1.0 to 1.0)
         * @param _loop Whether the audio should loop
         * @param _track The track handle to play the audio on
         */
        PlaybackHandle Play(float             _volume = 1.f,
                            float             _pitch  = 1.f,
                            float             _pan    = 0.f,
                            bool              _loop   = false,
                            const TrackHandle& _track = {}) const;

        /** Stop all audio associated with this handle. */
        void StopAll() const;
    };

} // namespace Audio

#endif // AUDIO_HPP_
