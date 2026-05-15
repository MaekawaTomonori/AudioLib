#ifndef Internal_HPP_
#define Internal_HPP_
#include "LoadDispatcher.hpp"
#include "Loader.hpp"
#include "Mixer.hpp"
#include "Repository.hpp"

namespace Audio::Internal {

    Repository*     GetRepository();
    Mixer*          GetMixer();
    LoadDispatcher* GetDispatcher();

} // namespace Audio::Internal
#endif // Internal_HPP_
