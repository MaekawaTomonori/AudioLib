#include "Loader.hpp"

#include <string>
#include <vector>

#include "vendor/miniaudio.h"

namespace Audio::Loader {

    namespace {
        constexpr ma_uint32 kOutputChannels  = 2;
        constexpr ma_uint32 kOutputSampleRate = 44100;
    }

    std::unique_ptr<Sound> Load(std::string_view _path) {
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, kOutputChannels, kOutputSampleRate);

        void*       pPcmData   = nullptr;
        ma_uint64   frameCount = 0;

        const ma_result result = ma_decode_file(
            std::string(_path).c_str(), &config, &frameCount, &pPcmData);

        if (result != MA_SUCCESS) {
            return std::make_unique<Sound>();
        }

        const ma_uint64 sampleCount = frameCount * kOutputChannels;
        std::vector<float> pcm(static_cast<float*>(pPcmData),
                               static_cast<float*>(pPcmData) + sampleCount);
        ma_free(pPcmData, nullptr);

        return std::make_unique<Sound>(std::move(pcm), kOutputSampleRate, kOutputChannels, frameCount);
    }

} // namespace Audio::Loader
