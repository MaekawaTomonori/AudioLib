#include "Internal.hpp"

namespace Audio {
    namespace {
        std::unique_ptr<Repository> repository = std::make_unique<Repository>();
        std::unique_ptr<Mixer>      mixer      = std::make_unique<Mixer>();
    }

    namespace Internal {
        Repository* GetRepository() { return repository.get(); }
        Mixer*      GetMixer()      { return mixer.get(); }
    }
} // namespace Audio
