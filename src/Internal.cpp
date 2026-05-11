#include "Internal.hpp"

namespace {
    std::unique_ptr<Audio::Internal::Repository>     repository = std::make_unique<Audio::Internal::Repository>();
    std::unique_ptr<Audio::Internal::Mixer>          mixer      = std::make_unique<Audio::Internal::Mixer>();
    std::unique_ptr<Audio::Internal::LoadDispatcher> dispatcher = std::make_unique<Audio::Internal::LoadDispatcher>(
        std::thread::hardware_concurrency()
    );
}

namespace Audio::Internal {
    Repository*     GetRepository() { return repository.get(); }
    Mixer*          GetMixer()      { return mixer.get(); }
    LoadDispatcher* GetDispatcher() { return dispatcher.get(); }
} // namespace Audio::Internal
