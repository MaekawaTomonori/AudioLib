#include "Internal.hpp"

namespace {
    std::unique_ptr<Audio::Internal::Repository> repository = std::make_unique<Audio::Internal::Repository>();
    std::unique_ptr<Audio::Internal::Mixer>      mixer      = std::make_unique<Audio::Internal::Mixer>();
}

namespace Audio::Internal {
    Repository* GetRepository() { return repository.get(); }
    Mixer*      GetMixer()      { return mixer.get(); }
} // namespace Audio::Internal
