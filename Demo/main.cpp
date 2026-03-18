#include "AudioLib.hpp"
#include <chrono>
#include <thread>

int main() {
    Audio::Initialize();
    Audio::Handle audio("fx.wav");
    auto p = audio.Play();

    while (p.IsPlaying()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Audio::Shutdown();
    return 0;
}
