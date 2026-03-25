#ifndef Loader_HPP_
#define Loader_HPP_
#include <memory>
#include <string_view>

#include "Sound.hpp"


namespace Audio::Internal::Loader {
    std::unique_ptr<Sound> Load(std::string_view _path);
} // namespace Audio::Internal::Loader

#endif // Loader_HPP_
