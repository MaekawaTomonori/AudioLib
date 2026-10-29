#ifndef StreamingVoice_HPP_
#define StreamingVoice_HPP_

#include <cstdint>
#include <memory>

#include "StreamingAsset.hpp"

namespace Audio::Internal {

    /** StreamingAsset の 1 回の再生。専用のデコーダと PCM リングバッファ（2 秒分）を持つ。
     *
     *  スレッドの分担（ロードマップ 3.5 の設計原則）:
     *  - オーディオスレッド: DataSource() の読み出しだけ。リングバッファから読むだけで、デコード・file IO・確保・ロックをしない。
     *    足りないときは無音を返して underrun を数える。最初の準備（prefetch）が終わるまでは、何も消費せず無音を返す。
     *  - refill ワーカー（StreamRefiller）: Refill() だけ。最初の呼び出しでデコーダを開いて 0.5 秒分を用意し、
     *    その後は残りが 25% を切るたびに満たす。ループ中は終端でデコーダを先頭へ戻して続ける。
     *  - API スレッド: 作成と破棄（破棄は再生（ma_sound）を外した後なので、オーディオスレッドからはもう読まれない）。
     */
    class StreamingVoice {
        struct Impl;
        std::unique_ptr<Impl> impl_;

    public:
        explicit StreamingVoice(std::shared_ptr<const StreamingAsset> _asset);
        ~StreamingVoice();
        StreamingVoice(const StreamingVoice&)            = delete;
        StreamingVoice& operator=(const StreamingVoice&) = delete;

        /** ma_sound_init_from_data_source に渡す ma_data_source*（miniaudio の型を見せないため void*）。 */
        void* DataSource();

        // --- Refill (worker only) ---

        bool NeedsRefill() const;
        void Refill();

        // --- State Query ---

        /** 最後まで読み終わった（ループしない）か、デコーダを開けなかった。 */
        bool     IsFinished() const;
        uint64_t Underruns() const;
    };

} // namespace Audio::Internal

#endif // StreamingVoice_HPP_
