#include "include/Handle.hpp"

#include "src/Internal.hpp"

namespace Audio {
    
    Handle::Handle(std::string_view _path) {
        auto repository = Internal::GetRepository();

        // Check if the audio is already registered
        if (repository->Contains(_path)) {
            handle_ = repository->FindId(_path);
            return;
        }

        // Load audio from file path
        auto sound = Loader::Load(_path);
        // Register the sound to Repository and get a handle
        handle_ = repository->Register(std::move(sound), _path);
    }

    PlaybackHandle Handle::Play(const float& _volume, const float& _pitch, const float& _pan, const bool& _loop, const TrackHandle& _track) const {
        const Sound* sound = Internal::GetRepository()->Find(handle_);
        if (!sound || !sound->IsValid()) return {};
        const uint64_t id = Internal::GetMixer()->Play(handle_, *sound, _track.GetId());
        PlaybackHandle handle(id);
        handle.SetVolume(_volume);
        handle.SetPitch(_pitch);
        handle.SetPan(_pan);
        handle.SetLoop(_loop);
        return handle;
    }

    void Handle::StopAll() const {
        Internal::GetMixer()->StopAll(handle_);
    }

} // namespace Handle