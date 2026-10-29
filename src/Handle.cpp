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

    // --- Loading ---

    // _name で登録済みならそれを使い、なければ _decode の結果（Static か Streaming、Loader::LoadedAsset）を _name で登録する
    template <class Decode>
    static void SubmitLoad(std::shared_ptr<Impl> _impl, std::string _name, Decode _decode);

    // --- State Query ---

    void WaitUntilReady() {
        std::unique_lock lock(mutex);
        cv.wait(lock, [this] { return state != LoadState::Loading; });
    }

    bool IsLoaded() const { return state == LoadState::Loaded; }
};

template <class Decode>
void Handle::Impl::SubmitLoad(std::shared_ptr<Impl> _impl, std::string _name, Decode _decode) {
    {
        std::scoped_lock lock(_impl->mutex);
        _impl->state  = LoadState::Loading;
        _impl->id     = 0;
        _impl->result = {};
    }

    Internal::GetDispatcher()->Submit([impl = std::move(_impl), name = std::move(_name), decode = std::move(_decode)]() mutable {
        auto* repo = Internal::GetRepository();

        // 既にロード済みなら即完了
        if (const uint64_t id = repo->FindId(name); id != 0) {
            std::scoped_lock lock(impl->mutex);
            impl->id     = id;
            impl->result = Result{true};
            impl->state  = LoadState::Loaded;
            impl->cv.notify_all();
            return;
        }

        auto loaded = decode();

        std::scoped_lock lock(impl->mutex);
        if (!loaded.IsValid()) {
            impl->result = Result{false, "Failed to load audio: " + name};
            impl->state  = LoadState::Failed;
        } else {
            impl->id = loaded.sound ? repo->Register(std::move(loaded.sound), name) : repo->Register(std::move(loaded.streaming), name);
            if (impl->id == 0) {
                impl->result = Result{false, "Failed to register audio: " + name};
                impl->state  = LoadState::Failed;
            } else {
                impl->result = Result{true};
                impl->state  = LoadState::Loaded;
            }
        }
        impl->cv.notify_all();
    });
}

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

void Handle::Load(const std::string& _path) const {
    Impl::SubmitLoad(impl_, _path, [_path] { return Internal::Loader::LoadAsset(_path); });
}

void Handle::Load(const std::string& _name, std::vector<uint8_t> _bytes) const {
    Impl::SubmitLoad(impl_, _name, [bytes = std::move(_bytes)]() mutable { return Internal::Loader::LoadAsset(std::move(bytes)); });
}

void Handle::Unload() const {
    std::unique_lock lock(impl_->mutex);
    impl_->cv.wait(lock, [this] { return impl_->state != Impl::LoadState::Loading; });
    if (impl_->IsLoaded()) Internal::GetRepository()->Unregister(impl_->id);
    impl_->state  = Impl::LoadState::Unloaded;
    impl_->id     = 0;
    impl_->result = {};
}

// --- State Query ---

bool Handle::IsReady() const {
    if (!impl_) return true;
    std::scoped_lock lock(impl_->mutex);
    return impl_->state != Impl::LoadState::Loading;
}

const Result& Handle::IsValid() const {
    impl_->WaitUntilReady();
    return impl_->result;
}

// --- Playback ---

PlaybackHandle Handle::Play(const float _volume, const float _pitch, const float _pan,
                            const bool _loop, const TrackHandle& _track) const {
    return Start(_volume, _pitch, _pan, _loop, _track, nullptr);
}

PlaybackHandle Handle::Play(const float _volume, const float _pitch, const float _pan, const Ears& _ears,
                            const bool _loop, const TrackHandle& _track) const {
    return Start(_volume, _pitch, _pan, _loop, _track, &_ears);
}

PlaybackHandle Handle::Start(const float _volume, const float _pitch, const float _pan,
                             const bool _loop, const TrackHandle& _track, const Ears* _ears) const {
    impl_->WaitUntilReady();
    if (!impl_->IsLoaded()) return {};

    // 長い音はロード時に Streaming になっている（LoadPolicy）。どちらでも PlaybackHandle の扱いは同じ
    const auto* repository = Internal::GetRepository();
    uint64_t id = 0;
    if (auto sound = repository->Find(impl_->id); sound && sound->IsValid()) {
        id = Internal::GetMixer()->Play(impl_->id, std::move(sound), _track.GetId(), _ears);
    } else if (auto streaming = repository->FindStreaming(impl_->id)) {
        id = Internal::GetMixer()->PlayStreaming(impl_->id, std::move(streaming), _track.GetId(), _ears);
    }
    if (id == 0) return {};

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
