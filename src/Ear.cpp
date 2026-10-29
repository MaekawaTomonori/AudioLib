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

        // 値の並び: 左右の音量、左右の遅れ（サンプル）、左右のカットオフ（Hz の 2 を底とする対数。近づけるとき耳に均等になる）
        enum Value { LeftVolume, RightVolume, LeftDelay, RightDelay, LeftCutoff, RightCutoff, Send, ValueCount };
        // カットオフが動いている間、ローパスの係数を作り直す間隔（サンプル）。32 では 10 ms で 5 オクターブ動かすと段差が出た
        constexpr uint32_t tuneEvery = 8;

        // 壊れた値（NaN、負）で音を壊さない
        float Volume(const float _volume) {
            return std::isfinite(_volume) ? std::max(_volume, 0.f) : 1.f;
        }
        float DelaySeconds(const float _delay) {
            return std::isfinite(_delay) ? std::clamp(_delay, 0.f, Ears::maxDelay) : 0.f;
        }
        float CutoffOctaves(const float _cutoff) {
            return std::log2(std::isnan(_cutoff) ? Ears::noCutoff : std::clamp(_cutoff, 20.f, Ears::noCutoff));
        }

        /** 2 次のローパス（RBJ の Audio EQ Cookbook、Q 0.707 = Butterworth）。転置直接形 II で、覚えておく値は 2 つ。
         *  y = b0·x + z1、z1 = b1·x − a1·y + z2、z2 = b2·x − a2·y（係数は a0 で割ってある）。 */
        struct Lowpass {
            float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
            float z1 = 0.f, z2 = 0.f;
            // 一度削り始めたら、次の Restart まで通し続ける（20 kHz でも素通しとは少し違うので、切り替えるたびに段差になる）
            bool on = false;
            // この回に削り始めた: 素の音から削った音へ、この回の間に混ぜ替える
            bool fading = false;
            // この回にカットオフが動く: tuneEvery ごとに係数を作り直す
            bool moving = false;

            void Tune(const float _cutoff, const float _sampleRate) {
                constexpr float q     = 0.70710678f;
                const float     w0    = 2.f * 3.14159265f * _cutoff / _sampleRate;
                const float     cosw  = std::cos(w0);
                const float     alpha = std::sin(w0) / (2.f * q);
                const float     a0    = 1.f + alpha;
                b0 = (1.f - cosw) / 2.f / a0;
                b1 = (1.f - cosw) / a0;
                b2 = b0;
                a1 = -2.f * cosw / a0;
                a2 = (1.f - alpha) / a0;
            }
            // 削り始める前に: 直前の値 _x がずっと続いていた（その出力も _x）ときの状態にしておく。0 から始めると段差になる
            void Prime(const float _x) {
                z2 = (b2 - a2) * _x;
                z1 = (b1 - a1) * _x + z2;
            }
            float Run(const float _x) {
                const float y = b0 * _x + z1;
                z1 = b1 * _x - a1 * y + z2;
                z2 = b2 * _x - a2 * y;
                return y;
            }
        };
    } // namespace

    struct Ear::Node {
        // miniaudio はノードを ma_node_base* として扱うので、先頭に置く
        ma_node_base base{};
        float        sampleRate = 48000.f;
        // これ以上のカットオフは削らない（20 kHz、低いサンプルレートではナイキストの手前）。2 を底とする対数
        float        openOctaves = 0.f;

        // --- API スレッドが書く ---
        std::array<std::atomic<float>, ValueCount> target{};
        std::atomic<bool>                          restart{true};

        // --- オーディオスレッドだけが触る ---
        std::array<float, ValueCount>                  current{};
        std::array<std::array<float, ringSize>, 2>     ring{};
        uint32_t                                       write = 0;
        std::array<Lowpass, 2>                         lowpass{};
        // 左右それぞれ、ローパスに入る前の直前の値（削り始めるときの Prime に使う）
        std::array<float, 2>                           last{};

        void Store(const Ears& _ears) {
            target[LeftVolume].store(Volume(_ears.leftVolume), std::memory_order_relaxed);
            target[RightVolume].store(Volume(_ears.rightVolume), std::memory_order_relaxed);
            target[LeftDelay].store(std::min(DelaySeconds(_ears.leftDelay) * sampleRate, maxDelayFrames), std::memory_order_relaxed);
            target[RightDelay].store(std::min(DelaySeconds(_ears.rightDelay) * sampleRate, maxDelayFrames), std::memory_order_relaxed);
            target[LeftCutoff].store(std::min(CutoffOctaves(_ears.leftCutoff), openOctaves), std::memory_order_relaxed);
            target[RightCutoff].store(std::min(CutoffOctaves(_ears.rightCutoff), openOctaves), std::memory_order_relaxed);
            target[Send].store(std::isnan(_ears.send) ? 0.f : std::clamp(_ears.send, 0.f, 1.f), std::memory_order_relaxed);
        }

        // 片方の耳の 1 サンプル: 遅らせ、削り、音量を掛ける。_t はこの回のうちのどこか（0 から 1）
        float Hear(const int _channel, const uint32_t _frame, const float _t, const std::array<float, ValueCount>& _now) {
            float sample = Delayed(_channel, _now[LeftDelay + _channel]);
            last[_channel] = sample;
            auto& filter   = lowpass[_channel];
            if (filter.on) {
                if (filter.moving && _frame % tuneEvery == 0) filter.Tune(std::exp2(_now[LeftCutoff + _channel]), sampleRate);
                const float cut = filter.Run(sample);
                sample          = filter.fading ? sample + (cut - sample) * _t : cut;
            }
            return sample * _now[LeftVolume + _channel];
        }

        // _delay サンプル前の値。整数でない遅れは、その前後の 2 つを割合で混ぜる（遅れ 0 なら今書いた値そのもの）
        float Delayed(const int _channel, const float _delay) const {
            const auto  whole = uint32_t(_delay);
            const float part  = _delay - float(whole);
            const float near  = ring[_channel][(write - whole) & ringMask];
            const float far   = ring[_channel][(write - whole - 1) & ringMask];
            return near + (far - near) * part;
        }

        void Process(const float* _in, float* _out, float* _send, const uint32_t _frames) {
            // restart を先に見る（Restart が書いた値を、そのあとで読むため）
            const bool fresh = restart.exchange(false, std::memory_order_acquire);
            std::array<float, ValueCount> to{};
            for (int i = 0; i < ValueCount; ++i) to[i] = target[i].load(std::memory_order_relaxed);
            if (fresh) {
                for (auto& channel : ring) channel.fill(0.f);
                lowpass = {};
                last    = {};
                current = to;
            }
            const std::array<float, ValueCount> from = current;
            // カットオフが上限より下になったら削り始める（それまでは何もせずに通すので、既定の Ears では値が変わらない）。
            // 再生の初めからなら最初のサンプルから削り、鳴っている途中なら、この回の間に素の音から混ぜ替える
            for (int channel = 0; channel < 2; ++channel) {
                auto&       filter = lowpass[channel];
                const float begin  = from[LeftCutoff + channel], end = to[LeftCutoff + channel];
                filter.fading      = false;
                filter.moving      = begin != end;
                if (!filter.on && (begin < openOctaves || end < openOctaves)) {
                    filter.Tune(std::exp2(begin), sampleRate);
                    filter.Prime(last[channel]);
                    filter.on     = true;
                    filter.fading = !fresh;
                } else if (filter.on && !filter.moving) {
                    filter.Tune(std::exp2(end), sampleRate);
                }
            }
            for (uint32_t frame = 0; frame < _frames; ++frame) {
                // この回の終わりに目標へ着くよう、1 サンプルごとに少しずつ近づける
                const float t = float(frame + 1) / float(_frames);
                std::array<float, ValueCount> now{};
                for (int i = 0; i < ValueCount; ++i) now[i] = from[i] + (to[i] - from[i]) * t;

                ring[0][write] = _in[frame * 2];
                ring[1][write] = _in[frame * 2 + 1];
                _out[frame * 2]     = Hear(0, frame, t, now);
                _out[frame * 2 + 1] = Hear(1, frame, t, now);
                write = (write + 1) & ringMask;
            }
            // 響きに送る分: 耳の音量・遅れ・カットオフの前の音に send を掛ける（響きは周り中から来るので、向きを付けない）
            if (from[Send] == 0.f && to[Send] == 0.f) {
                std::fill(_send, _send + _frames * 2, 0.f);
            } else {
                for (uint32_t frame = 0; frame < _frames; ++frame) {
                    const float amount = from[Send] + (to[Send] - from[Send]) * (float(frame + 1) / float(_frames));
                    _send[frame * 2]     = _in[frame * 2] * amount;
                    _send[frame * 2 + 1] = _in[frame * 2 + 1] * amount;
                }
            }
            current = to;
        }
    };

    namespace {
        void ProcessEar(ma_node* _node, const float** _framesIn, ma_uint32* _frameCountIn, float** _framesOut, ma_uint32* _frameCountOut) {
            (void)_frameCountIn; // 入力と出力の数は同じ（速さを変えないノード）
            // Node は先頭が ma_node_base なので、miniaudio から渡されたノードはそのまま Ear::Node を指している
            reinterpret_cast<Ear::Node*>(_node)->Process(_framesIn[0], _framesOut[0], _framesOut[1], *_frameCountOut);
        }

        // 入力 1・出力 2（耳ごとの音と、響きに送る分。miniaudio は 1 回処理して両方を取っておき、2 つ目はそこから読む）。
        // 音が止まっている間は処理しない（1 ms 以下の遅れの残りは、次の Restart で消す）
        ma_node_vtable earVtable = {ProcessEar, nullptr, 1, 2, 0};
    } // namespace

    Ear::Ear(std::unique_ptr<Node> _node) : node_(std::move(_node)) {}

    Ear::~Ear() {
        if (node_) ma_node_uninit(&node_->base, nullptr);
    }

    std::unique_ptr<Ear> Ear::Create(ma_engine* _engine) {
        if (!_engine || ma_engine_get_channels(_engine) != 2) return nullptr;

        auto node        = std::make_unique<Node>();
        node->sampleRate  = float(ma_engine_get_sample_rate(_engine));
        node->openOctaves = std::log2(std::min(Ears::noCutoff, node->sampleRate * 0.45f));
        node->Store({});

        constexpr ma_uint32 inputChannels[1]  = {2};
        constexpr ma_uint32 outputChannels[2] = {2, 2};
        ma_node_config      config            = ma_node_config_init();
        config.vtable                         = &earVtable;
        config.pInputChannels                 = inputChannels;
        config.pOutputChannels                = outputChannels;
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
