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

    PlaybackHandle Handle::Play(const float& _volume, const float& _pitch, const bool& _loop) const {
        const auto path = Internal::GetRepository()->FindName(handle_);
        if (path.empty()) return {};
        const uint64_t id = Internal::GetMixer()->Play(path);
        PlaybackHandle handle(id);
        handle.SetVolume(_volume);
        handle.SetPitch(_pitch);
        handle.SetLoop(_loop);
        return handle;
    }

} // namespace Handle