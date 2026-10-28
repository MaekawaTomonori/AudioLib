#include "Loader.hpp"

#include <string>
#include <vector>

#include "LoadPolicy.hpp"
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

    namespace {
        // 開いたデコーダから長さを読み、デコーダを閉じる
        AssetMeta ProbeOpened(ma_decoder& _decoder) {
            AssetMeta meta;
            ma_uint64 frames = 0;
            if (ma_decoder_get_length_in_pcm_frames(&_decoder, &frames) == MA_SUCCESS && frames > 0) {
                meta.sampleRate            = kOutputSampleRate;
                meta.channels              = kOutputChannels;
                meta.totalFrames           = frames;
                meta.durationSec           = static_cast<double>(frames) / kOutputSampleRate;
                meta.estimatedDecodedBytes = frames * kOutputChannels * sizeof(float);
                meta.probeSucceeded        = true;
            }
            ma_decoder_uninit(&_decoder);
            return meta;
        }
    }

    AssetMeta Probe(const std::string& _path) {
        const ma_decoder_config config = OutputConfig();
        ma_decoder decoder;
        if (ma_decoder_init_file(_path.c_str(), &config, &decoder) != MA_SUCCESS) return {};
        return ProbeOpened(decoder);
    }

    AssetMeta Probe(const std::span<const uint8_t> _bytes) {
        const ma_decoder_config config = OutputConfig();
        ma_decoder decoder;
        if (ma_decoder_init_memory(_bytes.data(), _bytes.size(), &config, &decoder) != MA_SUCCESS) return {};
        return ProbeOpened(decoder);
    }

    LoadedAsset LoadAsset(const std::string& _path) {
        LoadedAsset loaded;
        const AssetMeta meta = Probe(_path);
        if (LoadPolicy::Decide(meta) == LoadPolicy::Storage::Streaming) {
            loaded.streaming = std::make_unique<StreamingAsset>(StreamingAsset::FromFile(_path, meta));
        } else {
            loaded.sound = Load(_path);
        }
        return loaded;
    }

    LoadedAsset LoadAsset(std::vector<uint8_t> _bytes) {
        LoadedAsset loaded;
        const AssetMeta meta = Probe(_bytes);
        if (LoadPolicy::Decide(meta) == LoadPolicy::Storage::Streaming) {
            loaded.streaming = std::make_unique<StreamingAsset>(StreamingAsset::FromMemory(std::move(_bytes), meta));
        } else {
            loaded.sound = Load(_bytes);
        }
        return loaded;
    }

} // namespace Audio::Internal::Loader
