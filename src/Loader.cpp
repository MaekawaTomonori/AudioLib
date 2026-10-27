#include "Loader.hpp"

#include <string>
#include <vector>

#include "vendor/miniaudio.h"

namespace Audio::Internal::Loader {

    namespace {
        constexpr ma_uint32 kOutputChannels  = 2;
        constexpr ma_uint32 kOutputSampleRate = 44100;

        ma_decoder_config OutputConfig() {
            return ma_decoder_config_init(ma_format_f32, kOutputChannels, kOutputSampleRate);
        }

        std::unique_ptr<Sound> ToSound(const ma_result _result, void* _pcmData, const ma_uint64 _frameCount) {
            if (_result != MA_SUCCESS) {
                return std::make_unique<Sound>();
            }

            const ma_uint64 sampleCount = _frameCount * kOutputChannels;
            std::vector<float> pcm(static_cast<float*>(_pcmData),
                                   static_cast<float*>(_pcmData) + sampleCount);
            ma_free(_pcmData, nullptr);

            return std::make_unique<Sound>(std::move(pcm), kOutputSampleRate, kOutputChannels, _frameCount);
        }
    }

    std::unique_ptr<Sound> Load(const std::string& _path) {
        ma_decoder_config config = OutputConfig();

        void*       pPcmData   = nullptr;
        ma_uint64   frameCount = 0;

        const ma_result result = ma_decode_file(
            std::string(_path).c_str(), &config, &frameCount, &pPcmData);

        return ToSound(result, pPcmData, frameCount);
    }

    std::unique_ptr<Sound> Load(const std::span<const uint8_t> _bytes) {
        ma_decoder_config config = OutputConfig();

        void*       pPcmData   = nullptr;
        ma_uint64   frameCount = 0;

        const ma_result result = ma_decode_memory(
            _bytes.data(), _bytes.size(), &config, &frameCount, &pPcmData);

        return ToSound(result, pPcmData, frameCount);
    }

} // namespace Audio::Internal::Loader
