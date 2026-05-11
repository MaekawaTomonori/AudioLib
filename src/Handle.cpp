#include "include/Handle.hpp"

#include <condition_variable>
#include <mutex>

#include "src/Internal.hpp"

namespace Audio {

// --- Impl ---

struct Handle::Impl {
    enum class LoadState : uint8_t { Unloaded, Loading, Loaded, Failed };

    std::mutex              mutex;
    std::condition_variable cv;
    LoadState               state  = LoadState::Unloaded;
    uint64_t                id     = 0;
    Result                  result;

    void WaitUntilReady() {
        std::unique_lock lock(mutex);
        cv.wait(lock, [this] { return state != LoadState::Loading; });
    }

    bool IsLoaded() const { return state == LoadState::Loaded; }
};

// --- Construction ---

Handle::Handle()
    : impl_(std::make_shared<Impl>())
{}

Handle::Handle(const std::string& _path)
    : impl_(std::make_shared<Impl>())
{
    Load(_path);
}

// --- Loading ---

void Handle::Load(const std::string& _path) {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->state  = Impl::LoadState::Loading;
        impl_->id     = 0;
        impl_->result = {};
    }

    auto impl = impl_; // shared ownership for the lambda
    Internal::GetDispatcher()->Submit([impl, _path] {
        auto* repo = Internal::GetRepository();

        // 既にロード済みなら即完了
        if (const uint64_t id = repo->FindId(_path); id != 0) {
            std::lock_guard lock(impl->mutex);
            impl->id     = id;
            impl->result = Result{true};
            impl->state  = Impl::LoadState::Loaded;
            impl->cv.notify_all();
            return;
        }

        auto sound = Internal::Loader::Load(_path);

        std::lock_guard lock(impl->mutex);
        if (!sound || !sound->IsValid()) {
            impl->result = Result{false, "Failed to load audio: " + _path};
            impl->state  = Impl::LoadState::Failed;
        } else {
            impl->id = repo->Register(std::move(sound), _path);
            if (impl->id == 0) {
                impl->result = Result{false, "Failed to register audio: " + _path};
                impl->state  = Impl::LoadState::Failed;
            } else {
                impl->result = Result{true};
                impl->state  = Impl::LoadState::Loaded;
            }
        }
        impl->cv.notify_all();
    });
}

// --- State Query ---

bool Handle::IsReady() const {
    if (!impl_) return true;
    std::lock_guard lock(impl_->mutex);
    return impl_->state != Impl::LoadState::Loading;
}

const Result& Handle::IsValid() const {
    impl_->WaitUntilReady();
    return impl_->result;
}

// --- Playback ---

PlaybackHandle Handle::Play(const float _volume, const float _pitch, const float _pan,
                            const bool _loop, const TrackHandle& _track) const {
    impl_->WaitUntilReady();
    if (!impl_->IsLoaded()) return {};

    const auto* sound = Internal::GetRepository()->Find(impl_->id);
    if (!sound || !sound->IsValid()) return {};

    const uint64_t id = Internal::GetMixer()->Play(impl_->id, *sound, _track.GetId());
    PlaybackHandle handle(id);
    handle.SetVolume(_volume);
    handle.SetPitch(_pitch);
    handle.SetPan(_pan);
    handle.SetLoop(_loop);
    return handle;
}

void Handle::StopAll() const {
    impl_->WaitUntilReady();
    if (impl_->IsLoaded()) {
        Internal::GetMixer()->StopAll(impl_->id);
    }
}

} // namespace Audio
