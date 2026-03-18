#include "AudioLib.hpp"
#include <chrono>
#include <thread>

int main() {
    Audio::Initialize();
    const Audio::Handle audio("fx.wav");
    {
        auto p = audio.Play();
        while (p.IsPlaying()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    (void)audio;
    Audio::Shutdown();
    return 0;
}
