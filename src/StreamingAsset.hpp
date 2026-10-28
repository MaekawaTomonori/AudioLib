#ifndef StreamingAsset_HPP_
#define StreamingAsset_HPP_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "AssetMeta.hpp"

namespace Audio::Internal {

    /** 再生しながらデコードする音声アセット。PCM は持たず、デコーダを開くための元データだけを持つ。
     *  元データはファイルのパスか、エンコードされたままのバイト列（Handle::Load(name, bytes)）のどちらか。
     *  再生（StreamingVoice）ごとにここからデコーダを開くので、同じアセットを同時に何度でも鳴らせる。
     */
    class StreamingAsset {
        std::string          path_;
        std::vector<uint8_t> bytes_;
        bool                 fromMemory_ = false;
        AssetMeta            meta_;

    public:
        static StreamingAsset FromFile(std::string _path, const AssetMeta& _meta) {
            StreamingAsset asset;
            asset.path_ = std::move(_path);
            asset.meta_ = _meta;
            return asset;
        }
        static StreamingAsset FromMemory(std::vector<uint8_t> _bytes, const AssetMeta& _meta) {
            StreamingAsset asset;
            asset.bytes_      = std::move(_bytes);
            asset.fromMemory_ = true;
            asset.meta_       = _meta;
            return asset;
        }

        bool IsValid() const { return meta_.probeSucceeded && (fromMemory_ ? !bytes_.empty() : !path_.empty()); }

        bool                        IsFromMemory() const { return fromMemory_; }
        const std::string&          GetPath()      const { return path_; }
        const std::vector<uint8_t>& GetBytes()     const { return bytes_; }
        const AssetMeta&            GetMeta()      const { return meta_; }
    };

} // namespace Audio::Internal

#endif // StreamingAsset_HPP_
