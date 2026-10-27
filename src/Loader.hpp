#ifndef Loader_HPP_
#define Loader_HPP_
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "Sound.hpp"

namespace Audio::Internal::Loader {
    std::unique_ptr<Sound> Load(const std::string& _path);
    std::unique_ptr<Sound> Load(std::span<const uint8_t> _bytes);
} // namespace Audio::Internal::Loader

#endif // Loader_HPP_
