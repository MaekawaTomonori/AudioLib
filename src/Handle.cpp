#include "include/Handle.hpp"

#include "src/Internal.hpp"

namespace Audio {

namespace {

    // --- Resource Loading ---

    Result DoLoad(const std::string& _path, uint64_t& _outId) {
        auto* repository = Internal::GetRepository();

        if (repository->Contains(_path)) {
            _outId = repository->FindId(_path);
            return {true};
        }

        auto sound = Internal::Loader::Load(_path);
        if (!sound || !sound->IsValid()) {
            return {false, std::string("Failed to load audio: ") + _path};
        }

        _outId = repository->Register(std::move(sound), _path);
        if (_outId == 0) {
            return {false, std::string("Failed to register audio: ") + _path};
        }

        return {true};
    }

} // anonymous namespace

    Handle::Handle(const std::string& _path) {
        latestResult_ = DoLoad(_path, handle_);
    }

    Result Handle::Load(const std::string& _path) {
        latestResult_ = DoLoad(_path, handle_);
        return latestResult_;
    }

    PlaybackHandle Handle::Play(const float _volume, const float _pitch, const float _pan, const bool _loop, const TrackHandle& _track) const {
        auto sound = Internal::GetRepository()->Find(handle_);
        if (!sound || !sound->IsValid()) return {};
        const uint64_t id = Internal::GetMixer()->Play(handle_, std::move(sound), _track.GetId());
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

} // namespace Audio