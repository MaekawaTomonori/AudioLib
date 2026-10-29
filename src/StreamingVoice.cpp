#include "StreamingVoice.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

#include "PcmRingBuffer.hpp"
#include "vendor/miniaudio.h"

namespace Audio::Internal {

    namespace {
        // 出力形式は静的な Sound と同じ f32 / stereo / 44.1kHz
        constexpr ma_uint32 kChannels   = 2;
        constexpr ma_uint32 kSampleRate = 44100;
        // リングバッファ 2.0 秒、再生前の準備 0.5 秒、補充の単位 0.5 秒、補充を始める残り 25%
        constexpr uint64_t kRingFrames      = kSampleRate * 2;
        constexpr uint64_t kPrefetchFrames  = kSampleRate / 2;
        constexpr uint64_t kChunkFrames     = kSampleRate / 2;
        constexpr uint64_t kLowWaterFrames  = kRingFrames / 4;
    }

    struct StreamingVoice::Impl {
        // miniaudio の custom data source: ma_data_source_base を先頭に置く
        struct Source {
            ma_data_source_base base;
            Impl*               owner;
        };

        Source                                source{};
        std::shared_ptr<const StreamingAsset> asset;
        PcmRingBuffer                         ring{kRingFrames, kChannels};

        // --- Worker Only ---
        ma_decoder         decoder{};
        bool               decoderOpen = false;
        std::vector<float> chunk;

        // --- Shared Between Threads ---
        std::atomic<bool>     primed{false};
        std::atomic<bool>     decodedToEnd{false};
        std::atomic<bool>     failed{false};
        std::atomic<bool>     looping{false};
        std::atomic<uint64_t> underruns{0};
        std::atomic<uint64_t> cursor{0};

        explicit Impl(std::shared_ptr<const StreamingAsset> _asset);
        ~Impl();

        // --- Decoding (worker) ---
        bool OpenDecoder();
        void FillUntil(uint64_t _frames);

        // --- Reading (audio thread) ---
        ma_result Read(float* _out, ma_uint64 _frameCount, ma_uint64* _framesRead);

        // --- Data Source Callbacks ---
        static Impl&     OwnerOf(ma_data_source* _source) { return *static_cast<Source*>(_source)->owner; }
        static ma_result OnRead(ma_data_source* _source, void* _out, ma_uint64 _frameCount, ma_uint64* _framesRead);
        static ma_result OnSeek(ma_data_source*, ma_uint64) { return MA_NOT_IMPLEMENTED; }
        static ma_result OnGetDataFormat(ma_data_source*, ma_format* _format, ma_uint32* _channels, ma_uint32* _sampleRate,
                                         ma_channel* _channelMap, size_t _channelMapCap);
        static ma_result OnGetCursor(ma_data_source* _source, ma_uint64* _cursor);
        static ma_result OnGetLength(ma_data_source* _source, ma_uint64* _length);
        static ma_result OnSetLooping(ma_data_source* _source, ma_bool32 _looping);
        static ma_data_source_vtable vtable;
    };

    // --- Data Source Callbacks ---

    ma_result StreamingVoice::Impl::OnRead(ma_data_source* _source, void* _out, ma_uint64 _frameCount, ma_uint64* _framesRead) {
        return OwnerOf(_source).Read(static_cast<float*>(_out), _frameCount, _framesRead);
    }

    ma_result StreamingVoice::Impl::OnGetDataFormat(ma_data_source*, ma_format* _format, ma_uint32* _channels, ma_uint32* _sampleRate,
                                                    ma_channel* _channelMap, size_t _channelMapCap) {
        *_format     = ma_format_f32;
        *_channels   = kChannels;
        *_sampleRate = kSampleRate;
        ma_channel_map_init_standard(ma_standard_channel_map_default, _channelMap, _channelMapCap, kChannels);
        return MA_SUCCESS;
    }

    ma_result StreamingVoice::Impl::OnGetCursor(ma_data_source* _source, ma_uint64* _cursor) {
        *_cursor = OwnerOf(_source).cursor.load(std::memory_order_relaxed);
        return MA_SUCCESS;
    }

    ma_result StreamingVoice::Impl::OnGetLength(ma_data_source* _source, ma_uint64* _length) {
        *_length = OwnerOf(_source).asset->GetMeta().totalFrames;
        return MA_SUCCESS;
    }

    ma_result StreamingVoice::Impl::OnSetLooping(ma_data_source* _source, ma_bool32 _looping) {
        OwnerOf(_source).looping.store(_looping == MA_TRUE, std::memory_order_release);
        return MA_SUCCESS;
    }

    // ループと範囲はこのデータソースが自分で扱う（終端で MA_AT_END を返さずに、ワーカーが先頭から補充する）
    ma_data_source_vtable StreamingVoice::Impl::vtable = {
        OnRead, OnSeek, OnGetDataFormat, OnGetCursor, OnGetLength, OnSetLooping,
        MA_DATA_SOURCE_SELF_MANAGED_RANGE_AND_LOOP_POINT
    };

    // --- Impl ---

    StreamingVoice::Impl::Impl(std::shared_ptr<const StreamingAsset> _asset)
        :asset(std::move(_asset)) {
        source.owner = this;
        ma_data_source_config config = ma_data_source_config_init();
        config.vtable = &vtable;
        if (ma_data_source_init(&config, &source.base) != MA_SUCCESS) failed = true;
    }

    StreamingVoice::Impl::~Impl() {
        if (decoderOpen) ma_decoder_uninit(&decoder);
        ma_data_source_uninit(&source.base);
    }

    bool StreamingVoice::Impl::OpenDecoder() {
        const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, kChannels, kSampleRate);
        const ma_result result = asset->IsFromMemory()
            ? ma_decoder_init_memory(asset->GetBytes().data(), asset->GetBytes().size(), &config, &decoder)
            : ma_decoder_init_file(asset->GetPath().c_str(), &config, &decoder);
        decoderOpen = result == MA_SUCCESS;
        if (decoderOpen) chunk.resize(static_cast<size_t>(kChunkFrames * kChannels));
        return decoderOpen;
    }

    // リングバッファに _frames 以上入るまで（入りきるだけ）デコードして書き込む
    void StreamingVoice::Impl::FillUntil(const uint64_t _frames) {
        bool restarted = false;
        while (ring.AvailableFrames() < _frames) {
            const uint64_t room  = ring.CapacityFrames() - ring.AvailableFrames();
            const uint64_t count = std::min(room, kChunkFrames);
            if (count == 0) return;
            ma_uint64 decoded = 0;
            const ma_result result = ma_decoder_read_pcm_frames(&decoder, chunk.data(), count, &decoded);
            ring.Write(chunk.data(), decoded);
            if (decoded > 0) restarted = false;
            if (result == MA_SUCCESS && decoded == count) continue;

            // 終端: ループ中なら先頭へ戻して続ける。戻した直後にも読めなければ壊れているので止める
            if (!looping.load(std::memory_order_acquire)) {
                decodedToEnd = true;
                return;
            }
            if (restarted || ma_decoder_seek_to_pcm_frame(&decoder, 0) != MA_SUCCESS) {
                failed = true;
                return;
            }
            restarted = true;
        }
    }

    ma_result StreamingVoice::Impl::Read(float* _out, const ma_uint64 _frameCount, ma_uint64* _framesRead) {
        const size_t bytes = static_cast<size_t>(_frameCount * kChannels) * sizeof(float);
        if (failed.load(std::memory_order_acquire) && ring.AvailableFrames() == 0) {
            *_framesRead = 0;
            return MA_AT_END;
        }
        // 準備が終わるまでは時間だけ進めて頭を欠かさない
        if (!primed.load(std::memory_order_acquire)) {
            std::memset(_out, 0, bytes);
            *_framesRead = _frameCount;
            return MA_SUCCESS;
        }
        // 終端の印を先に読む: ワーカーは最後のフレームを書いてから印を立てるので、印が見えたら残りはもう全部バッファにある
        const bool ended = decodedToEnd.load(std::memory_order_acquire) && !looping.load(std::memory_order_acquire);
        const uint64_t got = ring.Read(_out, _frameCount);
        cursor.fetch_add(got, std::memory_order_relaxed);
        if (got == _frameCount) {
            *_framesRead = got;
            return MA_SUCCESS;
        }
        if (ended) {
            *_framesRead = got;
            return got == 0 ? MA_AT_END : MA_SUCCESS;
        }
        // 補充が間に合わなかった: 残りは無音
        std::memset(_out + got * kChannels, 0, static_cast<size_t>((_frameCount - got) * kChannels) * sizeof(float));
        underruns.fetch_add(1, std::memory_order_relaxed);
        *_framesRead = _frameCount;
        return MA_SUCCESS;
    }

    // --- StreamingVoice ---

    StreamingVoice::StreamingVoice(std::shared_ptr<const StreamingAsset> _asset)
        :impl_(std::make_unique<Impl>(std::move(_asset))) {
    }

    StreamingVoice::~StreamingVoice() = default;

    void* StreamingVoice::DataSource() { return &impl_->source.base; }

    bool StreamingVoice::NeedsRefill() const {
        const Impl& impl = *impl_;
        if (impl.failed.load(std::memory_order_acquire)) return false;
        if (!impl.primed.load(std::memory_order_acquire)) return true;
        if (impl.decodedToEnd.load(std::memory_order_acquire)) return impl.looping.load(std::memory_order_acquire);
        return impl.ring.AvailableFrames() < kLowWaterFrames;
    }

    void StreamingVoice::Refill() {
        Impl& impl = *impl_;
        if (!impl.decoderOpen && !impl.OpenDecoder()) {
            impl.failed = true;
            return;
        }
        // ループが後から有効になったら、終端から先頭へ戻して続ける
        if (impl.decodedToEnd && impl.looping.load(std::memory_order_acquire)) {
            if (ma_decoder_seek_to_pcm_frame(&impl.decoder, 0) != MA_SUCCESS) {
                impl.failed = true;
                return;
            }
            impl.decodedToEnd = false;
        }
        if (!impl.primed) {
            impl.FillUntil(kPrefetchFrames);
            impl.primed.store(true, std::memory_order_release);
        }
        impl.FillUntil(impl.ring.CapacityFrames());
    }

    bool StreamingVoice::IsFinished() const {
        return impl_->failed.load(std::memory_order_acquire) ||
               (impl_->decodedToEnd.load(std::memory_order_acquire) && !impl_->looping.load(std::memory_order_acquire));
    }

    uint64_t StreamingVoice::Underruns() const { return impl_->underruns.load(std::memory_order_relaxed); }

} // namespace Audio::Internal
