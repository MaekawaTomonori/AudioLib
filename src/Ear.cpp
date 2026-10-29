#include "Ear.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

#include "vendor/miniaudio.h"

namespace Audio::Internal {

    namespace {
        // 左右それぞれのリングバッファの長さ（2 のべき乗、添字は & で回す）。192 kHz でも 1 ms（192 サンプル）が入る
        constexpr uint32_t ringSize = 256;
        constexpr uint32_t ringMask = ringSize - 1;
        // 線形補間で隣も読むので、遅れは ringSize - 2 サンプルまで
        constexpr float maxDelayFrames = float(ringSize - 2);

        // 値の並び: 左の音量、右の音量、左の遅れ（サンプル）、右の遅れ（サンプル）
        enum Value { LeftVolume, RightVolume, LeftDelay, RightDelay, ValueCount };

        // 壊れた値（NaN、負）で音を壊さない
        float Volume(const float _volume) {
            return std::isfinite(_volume) ? std::max(_volume, 0.f) : 1.f;
        }
        float DelaySeconds(const float _delay) {
            return std::isfinite(_delay) ? std::clamp(_delay, 0.f, Ears::maxDelay) : 0.f;
        }
    } // namespace

    struct Ear::Node {
        // miniaudio はノードを ma_node_base* として扱うので、先頭に置く
        ma_node_base base{};
        float        sampleRate = 48000.f;

        // --- API スレッドが書く ---
        std::array<std::atomic<float>, ValueCount> target{};
        std::atomic<bool>                          restart{true};

        // --- オーディオスレッドだけが触る ---
        std::array<float, ValueCount>                  current{};
        std::array<std::array<float, ringSize>, 2>     ring{};
        uint32_t                                       write = 0;

        void Store(const Ears& _ears) {
            target[LeftVolume].store(Volume(_ears.leftVolume), std::memory_order_relaxed);
            target[RightVolume].store(Volume(_ears.rightVolume), std::memory_order_relaxed);
            target[LeftDelay].store(std::min(DelaySeconds(_ears.leftDelay) * sampleRate, maxDelayFrames), std::memory_order_relaxed);
            target[RightDelay].store(std::min(DelaySeconds(_ears.rightDelay) * sampleRate, maxDelayFrames), std::memory_order_relaxed);
        }

        // _delay サンプル前の値。整数でない遅れは、その前後の 2 つを割合で混ぜる（遅れ 0 なら今書いた値そのもの）
        float Delayed(const int _channel, const float _delay) const {
            const auto  whole = uint32_t(_delay);
            const float part  = _delay - float(whole);
            const float near  = ring[_channel][(write - whole) & ringMask];
            const float far   = ring[_channel][(write - whole - 1) & ringMask];
            return near + (far - near) * part;
        }

        void Process(const float* _in, float* _out, const uint32_t _frames) {
            // restart を先に見る（Restart が書いた値を、そのあとで読むため）
            const bool fresh = restart.exchange(false, std::memory_order_acquire);
            std::array<float, ValueCount> to{};
            for (int i = 0; i < ValueCount; ++i) to[i] = target[i].load(std::memory_order_relaxed);
            if (fresh) {
                for (auto& channel : ring) channel.fill(0.f);
                current = to;
            }
            const std::array<float, ValueCount> from = current;
            for (uint32_t frame = 0; frame < _frames; ++frame) {
                // この回の終わりに目標へ着くよう、1 サンプルごとに少しずつ近づける
                const float t = float(frame + 1) / float(_frames);
                std::array<float, ValueCount> now{};
                for (int i = 0; i < ValueCount; ++i) now[i] = from[i] + (to[i] - from[i]) * t;

                ring[0][write] = _in[frame * 2];
                ring[1][write] = _in[frame * 2 + 1];
                _out[frame * 2]     = Delayed(0, now[LeftDelay]) * now[LeftVolume];
                _out[frame * 2 + 1] = Delayed(1, now[RightDelay]) * now[RightVolume];
                write = (write + 1) & ringMask;
            }
            current = to;
        }
    };

    namespace {
        void ProcessEar(ma_node* _node, const float** _framesIn, ma_uint32* _frameCountIn, float** _framesOut, ma_uint32* _frameCountOut) {
            (void)_frameCountIn; // 入力と出力の数は同じ（速さを変えないノード）
            // Node は先頭が ma_node_base なので、miniaudio から渡されたノードはそのまま Ear::Node を指している
            reinterpret_cast<Ear::Node*>(_node)->Process(_framesIn[0], _framesOut[0], *_frameCountOut);
        }

        // 入力 1・出力 1。音が止まっている間は処理しない（1 ms 以下の遅れの残りは、次の Restart で消す）
        ma_node_vtable earVtable = {ProcessEar, nullptr, 1, 1, 0};
    } // namespace

    Ear::Ear(std::unique_ptr<Node> _node) : node_(std::move(_node)) {}

    Ear::~Ear() {
        if (node_) ma_node_uninit(&node_->base, nullptr);
    }

    std::unique_ptr<Ear> Ear::Create(ma_engine* _engine) {
        if (!_engine || ma_engine_get_channels(_engine) != 2) return nullptr;

        auto node        = std::make_unique<Node>();
        node->sampleRate = float(ma_engine_get_sample_rate(_engine));
        node->Store({});

        constexpr ma_uint32 channels[1] = {2};
        ma_node_config      config      = ma_node_config_init();
        config.vtable                   = &earVtable;
        config.pInputChannels           = channels;
        config.pOutputChannels          = channels;
        if (ma_node_init(ma_engine_get_node_graph(_engine), &config, nullptr, &node->base) != MA_SUCCESS) return nullptr;
        return std::unique_ptr<Ear>(new Ear(std::move(node)));
    }

    void* Ear::AsNode() {
        return &node_->base;
    }

    void Ear::Set(const Ears& _ears) {
        node_->Store(_ears);
    }

    void Ear::Restart(const Ears& _ears) {
        node_->Store(_ears);
        node_->restart.store(true, std::memory_order_release);
    }

} // namespace Audio::Internal
