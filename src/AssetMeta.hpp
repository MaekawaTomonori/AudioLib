#ifndef AssetMeta_HPP_
#define AssetMeta_HPP_

#include <cstdint>

namespace Audio::Internal {

    /** フルデコードの前にヘッダだけを読んで分かる音声の情報（Loader::Probe の結果）。
     *  フレーム数は出力形式（f32 / stereo / 44.1kHz）に直した値。
     */
    struct AssetMeta {
        uint32_t sampleRate            = 0;
        uint32_t channels              = 0;
        uint64_t totalFrames           = 0;
        double   durationSec           = 0.0;
        uint64_t estimatedDecodedBytes = 0;
        bool     probeSucceeded        = false;
    };

} // namespace Audio::Internal

#endif // AssetMeta_HPP_
