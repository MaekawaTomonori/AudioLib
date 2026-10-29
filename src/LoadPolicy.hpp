#ifndef LoadPolicy_HPP_
#define LoadPolicy_HPP_

#include <cstdint>

#include "AssetMeta.hpp"

namespace Audio::Internal::LoadPolicy {

    /** 全 PCM を持つ（Static）か、再生しながらデコードする（Streaming）か。 */
    enum class Storage : uint8_t { Static, Streaming };

    /** この長さか大きさを超える音は Streaming にする（ロードマップ 3.5-1）。 */
    inline constexpr double   kStreamingMinDurationSec  = 10.0;
    inline constexpr uint64_t kStreamingMinDecodedBytes = 16ull * 1024ull * 1024ull;

    /** 長さの分からない音（Probe 失敗など）は Static にする。フルデコードが失敗すれば、従来どおりロード失敗になる。 */
    inline Storage Decide(const AssetMeta& _meta) {
        if (!_meta.probeSucceeded || _meta.totalFrames == 0) return Storage::Static;
        if (_meta.durationSec >= kStreamingMinDurationSec || _meta.estimatedDecodedBytes >= kStreamingMinDecodedBytes) {
            return Storage::Streaming;
        }
        return Storage::Static;
    }

} // namespace Audio::Internal::LoadPolicy

#endif // LoadPolicy_HPP_
