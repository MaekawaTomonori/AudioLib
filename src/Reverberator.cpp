#include "Reverberator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

#include "vendor/miniaudio.h"

namespace Audio::Internal {

    namespace {
        constexpr int lineCount = 8;
        // 48 kHz でのラインの長さ（サンプル、30〜60 ms）。たがいに素なので、はね返りが同じ時刻に重ならない。ほかのレートでは比で伸び縮みさせる
        constexpr std::array<int, lineCount> lineLengthsAt48k = {1433, 1601, 1867, 2053, 2251, 2399, 2617, 2903};
        // damping 1 のときの 1 次ローパスの係数（1 に近いほど高い音がよく消える）
        constexpr float maxDampingPole = 0.85f;
        // 響きが止まったと見なす大きさ（−100 dB）と、十分小さい値を 0 にする大きさ（デノーマルを避ける）
        constexpr float quietLevel = 1e-5f;
        constexpr float tinyLevel  = 1e-20f;

        // 値の並び: 響きの長さ（秒）、damping、プリディレイ（サンプル）、音量
        enum Value { Decay, Damping, Predelay, Volume, ValueCount };

        float Clamped(const float _value, const float _low, const float _high, const float _broken) {
            return std::isnan(_value) ? _broken : std::clamp(_value, _low, _high);
        }

        // 長さ 8 のアダマール変換（直交になるよう 1/√8 を掛ける）。ラインの出力を全部のラインに均等に混ぜる
        void Hadamard(std::array<float, lineCount>& _values) {
            for (int span = 1; span < lineCount; span *= 2)
                for (int i = 0; i < lineCount; i += span * 2)
                    for (int j = i; j < i + span; ++j) {
                        const float a = _values[j], b = _values[j + span];
                        _values[j]        = a + b;
                        _values[j + span] = a - b;
                    }
            for (auto& value : _values) value *= 0.35355339f;
        }

        float Flushed(const float _value) {
            return std::abs(_value) < tinyLevel ? 0.f : _value;
        }
    } // namespace

    struct Reverberator::Node {
        // miniaudio はノードを ma_node_base* として扱うので、先頭に置く
        ma_node_base base{};
        float        sampleRate = 48000.f;

        // --- API スレッドが書く ---
        std::array<std::atomic<float>, ValueCount> target{};

        // --- オーディオスレッドだけが触る（バッファの確保は Create で） ---
        std::array<float, ValueCount>                current{};
        // プリディレイ: 左右のリングバッファ（2 のべき乗）
        std::array<std::vector<float>, 2>            early;
        uint32_t                                     earlyMask  = 0;
        uint32_t                                     earlyWrite = 0;
        float                                        maxPredelayFrames = 0.f;
        // FDN の 8 本: ちょうどその長さの輪で、読む位置と書く位置は同じ（読んでから書く）
        std::array<std::vector<float>, lineCount>    lines;
        std::array<uint32_t, lineCount>              position{};
        std::array<float, lineCount>                 damped{};
        // 止まっている（バッファは 0）。入力が来るまで計算しない
        bool                                         asleep = true;
        // 入力が続けて無音だったサンプルの数（プリディレイの中に残りがないと分かるまで眠らない）
        uint32_t                                     quietFrames = 0;

        void Store(const Reverb& _reverb) {
            target[Decay].store(Clamped(_reverb.decay, 0.1f, 20.f, 1.5f), std::memory_order_relaxed);
            target[Damping].store(Clamped(_reverb.damping, 0.f, 1.f, 0.5f), std::memory_order_relaxed);
            target[Predelay].store(Clamped(_reverb.predelay, 0.f, Reverb::maxPredelay, 0.f) * sampleRate, std::memory_order_relaxed);
            target[Volume].store(std::isfinite(_reverb.volume) ? std::max(_reverb.volume, 0.f) : 0.f, std::memory_order_relaxed);
        }

        // 響きの長さ _decay（秒）で、ライン _line を 1 回りしたときに掛ける割合
        float Feedback(const int _line, const float _decay) const {
            return std::pow(10.f, -3.f * float(lines[_line].size()) / (_decay * sampleRate));
        }

        float Early(const int _channel, const float _delay) const {
            const auto  whole = uint32_t(_delay);
            const float part  = _delay - float(whole);
            const float near  = early[_channel][(earlyWrite - whole) & earlyMask];
            const float far   = early[_channel][(earlyWrite - whole - 1) & earlyMask];
            return near + (far - near) * part;
        }

        void Sleep() {
            for (auto& channel : early) std::fill(channel.begin(), channel.end(), 0.f);
            for (auto& line : lines) std::fill(line.begin(), line.end(), 0.f);
            damped = {};
            asleep = true;
        }

        void Process(const float* _in, float* _out, const uint32_t _frames) {
            std::array<float, ValueCount> to{};
            for (int i = 0; i < ValueCount; ++i) to[i] = target[i].load(std::memory_order_relaxed);

            bool silent = true;
            if (_in)
                for (uint32_t i = 0; i < _frames * 2 && silent; ++i) silent = _in[i] == 0.f;
            quietFrames = silent ? quietFrames + _frames : 0;

            // 音が 0 のまま、または眠っていて入力も無音なら、何も計算しない
            if ((to[Volume] == 0.f && current[Volume] == 0.f) || (asleep && silent)) {
                if (!asleep) Sleep();
                current = to;
                std::fill(_out, _out + _frames * 2, 0.f);
                return;
            }
            if (asleep) {
                // 眠っている間に変わった値は、近づけずにそのまま使う
                asleep  = false;
                current = to;
            }

            const std::array<float, ValueCount> from = current;
            // プリディレイは速く動かすと響きの高さが揺れるので、1 回にこの回の長さの 5% まで
            const float step = 0.05f * float(_frames);
            to[Predelay]     = std::clamp(to[Predelay], from[Predelay] - step, from[Predelay] + step);
            std::array<float, lineCount> feedbackFrom{}, feedbackTo{};
            for (int line = 0; line < lineCount; ++line) {
                feedbackFrom[line] = Feedback(line, from[Decay]);
                feedbackTo[line]   = Feedback(line, to[Decay]);
            }

            float loudest = 0.f;
            for (uint32_t frame = 0; frame < _frames; ++frame) {
                const float t        = float(frame + 1) / float(_frames);
                const float pole     = (from[Damping] + (to[Damping] - from[Damping]) * t) * maxDampingPole;
                const float predelay = from[Predelay] + (to[Predelay] - from[Predelay]) * t;
                const float volume   = from[Volume] + (to[Volume] - from[Volume]) * t;

                early[0][earlyWrite] = _in ? _in[frame * 2] : 0.f;
                early[1][earlyWrite] = _in ? _in[frame * 2 + 1] : 0.f;
                const float left = Early(0, predelay), right = Early(1, predelay);
                earlyWrite       = (earlyWrite + 1) & earlyMask;

                // 各ラインの出口: 読んで、1 次ローパスで高い音を少し削り（damping）、アダマールで混ぜる
                std::array<float, lineCount> out{}, mixed{};
                for (int line = 0; line < lineCount; ++line) {
                    out[line]    = lines[line][position[line]];
                    damped[line] = Flushed(out[line] + (damped[line] - out[line]) * pole);
                    mixed[line]  = damped[line];
                    loudest      = std::max(loudest, std::abs(out[line]));
                }
                Hadamard(mixed);
                // 戻す: 1 回りの割合を掛け、左の入力を偶数番、右を奇数番に足す
                for (int line = 0; line < lineCount; ++line) {
                    const float feedback = feedbackFrom[line] + (feedbackTo[line] - feedbackFrom[line]) * t;
                    lines[line][position[line]] = Flushed(mixed[line] * feedback + (line % 2 == 0 ? left : right));
                    position[line] = position[line] + 1 == lines[line].size() ? 0 : position[line] + 1;
                }
                // 取り出す: 左は偶数番、右は奇数番を、符号を交互にして足す（同じ響きでも左右で違って聞こえる）
                _out[frame * 2]     = (out[0] - out[2] + out[4] - out[6]) * 0.5f * volume;
                _out[frame * 2 + 1] = (out[1] - out[3] + out[5] - out[7]) * 0.5f * volume;
            }
            current = to;

            // 入力がプリディレイより長く無音で、響きも消えたら眠る
            if (quietFrames > uint32_t(maxPredelayFrames) + _frames && loudest < quietLevel) Sleep();
        }
    };

    namespace {
        void ProcessReverb(ma_node* _node, const float** _framesIn, ma_uint32* _frameCountIn, float** _framesOut, ma_uint32* _frameCountOut) {
            (void)_frameCountIn; // 入力と出力の数は同じ
            // 何もつながっていないときは入力が nullptr（ALLOW_NULL_INPUT）
            reinterpret_cast<Reverberator::Node*>(_node)->Process(_framesIn ? _framesIn[0] : nullptr, _framesOut[0], *_frameCountOut);
        }

        // 入力 1（すべての Ear の送る分が足し合わされて入る）・出力 1。響きの残りがあるので、入力がなくても処理を続ける
        ma_node_vtable reverbVtable = {ProcessReverb, nullptr, 1, 1, MA_NODE_FLAG_CONTINUOUS_PROCESSING | MA_NODE_FLAG_ALLOW_NULL_INPUT};
    } // namespace

    Reverberator::Reverberator(std::unique_ptr<Node> _node) : node_(std::move(_node)) {}

    Reverberator::~Reverberator() {
        if (node_) ma_node_uninit(&node_->base, nullptr);
    }

    std::unique_ptr<Reverberator> Reverberator::Create(ma_engine* _engine) {
        if (!_engine || ma_engine_get_channels(_engine) != 2) return nullptr;

        auto node        = std::make_unique<Node>();
        node->sampleRate = float(ma_engine_get_sample_rate(_engine));
        node->Store({});
        for (int i = 0; i < ValueCount; ++i) node->current[i] = node->target[i].load(std::memory_order_relaxed);

        node->maxPredelayFrames = Reverb::maxPredelay * node->sampleRate;
        uint32_t size           = 1;
        while (size < uint32_t(node->maxPredelayFrames) + 2) size *= 2;
        for (auto& channel : node->early) channel.assign(size, 0.f);
        node->earlyMask = size - 1;
        for (int line = 0; line < lineCount; ++line) {
            const auto length = std::max(1L, std::lround(float(lineLengthsAt48k[line]) * node->sampleRate / 48000.f));
            node->lines[line].assign(size_t(length), 0.f);
        }

        constexpr ma_uint32 channels[1] = {2};
        ma_node_config      config      = ma_node_config_init();
        config.vtable                   = &reverbVtable;
        config.pInputChannels           = channels;
        config.pOutputChannels          = channels;
        if (ma_node_init(ma_engine_get_node_graph(_engine), &config, nullptr, &node->base) != MA_SUCCESS) return nullptr;
        auto reverberator = std::unique_ptr<Reverberator>(new Reverberator(std::move(node)));
        if (ma_node_attach_output_bus(&reverberator->node_->base, 0, ma_engine_get_endpoint(_engine), 0) != MA_SUCCESS) return nullptr;
        return reverberator;
    }

    void* Reverberator::AsNode() {
        return &node_->base;
    }

    void Reverberator::Set(const Reverb& _reverb) {
        node_->Store(_reverb);
    }

} // namespace Audio::Internal
