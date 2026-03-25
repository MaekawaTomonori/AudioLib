#ifndef Internal_HPP_
#define Internal_HPP_
#include "Mixer.hpp"
#include "Repository.hpp"
#include "Loader.hpp"

namespace Audio::Internal {

    Repository* GetRepository();
    Mixer*      GetMixer();

} // namespace Audio::Internal
#endif // Internal_HPP_
