#ifndef PcmRingBuffer_HPP_
#define PcmRingBuffer_HPP_

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Audio::Internal {

    /** 書き手 1 つ・読み手 1 つ（SPSC）の、PCM フレームの固定長リングバッファ。
     *  書き手（refill ワーカー）と読み手（オーディオスレッド）はロックせずに同時に使える。
     *  確保はコンストラクタだけで、Write / Read はメモリを確保しない（オーディオスレッドで呼べる）。
     *
     *  位置は増え続けるフレーム数で持ち、容量で割った余りを配列の位置にする。
     *  書いた数 - 読んだ数 = 中にある数。書き手だけが written_ を、読み手だけが read_ を進める。
     *  release で進め acquire で読むので、相手が見る数が増えたときには、その分のフレームはもう書かれている。
     */
    class PcmRingBuffer {
        std::vector<float>    samples_;
        uint64_t              capacityFrames_ = 0;
        uint32_t              channels_       = 0;
        std::atomic<uint64_t> written_{0};
        std::atomic<uint64_t> read_{0};

    public:
        PcmRingBuffer(const uint64_t _capacityFrames, const uint32_t _channels)
            :samples_(static_cast<size_t>(_capacityFrames * _channels)),
            capacityFrames_(_capacityFrames),
            channels_(_channels) {
        }

        uint64_t CapacityFrames() const { return capacityFrames_; }

        /** 中にあるフレーム数（どちらのスレッドからも呼べる。相手が動いている間はおおよその値）。 */
        uint64_t AvailableFrames() const {
            return written_.load(std::memory_order_acquire) - read_.load(std::memory_order_acquire);
        }

        /** 書き手専用: 空いているところに書けるだけ書き、書いたフレーム数を返す。 */
        uint64_t Write(const float* _frames, const uint64_t _frameCount) {
            const uint64_t written = written_.load(std::memory_order_relaxed);
            const uint64_t free    = capacityFrames_ - (written - read_.load(std::memory_order_acquire));
            const uint64_t count   = std::min(free, _frameCount);
            ForEachSpan(written, count, [&](float* _ring, const uint64_t _done, const size_t _bytes) {
                std::memcpy(_ring, _frames + _done * channels_, _bytes);
            });
            written_.store(written + count, std::memory_order_release);
            return count;
        }

        /** 読み手専用: あるだけ読み、読んだフレーム数を返す。 */
        uint64_t Read(float* _frames, const uint64_t _frameCount) {
            const uint64_t read  = read_.load(std::memory_order_relaxed);
            const uint64_t count = std::min(written_.load(std::memory_order_acquire) - read, _frameCount);
            ForEachSpan(read, count, [&](float* _ring, const uint64_t _done, const size_t _bytes) {
                std::memcpy(_frames + _done * channels_, _ring, _bytes);
            });
            read_.store(read + count, std::memory_order_release);
            return count;
        }

    private:
        // --- Storage ---

        // 位置 _at からの _count フレームを、配列の終わりで折り返して高々 2 つの連続した区間に分け、
        // 区間ごとに (配列内の先頭, そこまでに済んだフレーム数, 区間のバイト数) を渡す
        template <class Span>
        void ForEachSpan(const uint64_t _at, const uint64_t _count, Span _span) {
            uint64_t done = 0;
            while (done < _count) {
                const uint64_t slot  = (_at + done) % capacityFrames_;
                const uint64_t chunk = std::min(_count - done, capacityFrames_ - slot);
                _span(samples_.data() + slot * channels_, done, static_cast<size_t>(chunk * channels_) * sizeof(float));
                done += chunk;
            }
        }
    };

} // namespace Audio::Internal

#endif // PcmRingBuffer_HPP_
