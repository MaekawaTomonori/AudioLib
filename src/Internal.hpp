#ifndef Internal_HPP_
#define Internal_HPP_
#include "Mixer.hpp"
#include "Repository.hpp"
#include "Loader.hpp"

namespace Audio {
    namespace Internal {

        Repository* GetRepository();
        Mixer* GetMixer();

    }; // namespace Internal
} // namespace Handle
#endif // Internal_HPP_
