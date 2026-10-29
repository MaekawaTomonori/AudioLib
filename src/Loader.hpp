#ifndef Loader_HPP_
#define Loader_HPP_

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "AssetMeta.hpp"
#include "Sound.hpp"
#include "StreamingAsset.hpp"

namespace Audio::Internal::Loader {

    /** ロードの結果。どちらか一方だけが入る（失敗時はどちらも空か無効）。 */
    struct LoadedAsset {
        std::unique_ptr<Sound>          sound;
        std::unique_ptr<StreamingAsset> streaming;

        bool IsValid() const { return (sound && sound->IsValid()) || (streaming && streaming->IsValid()); }
    };

    // --- Full Decode ---

    std::unique_ptr<Sound> Load(const std::string& _path);
    std::unique_ptr<Sound> Load(std::span<const uint8_t> _bytes);

    // --- Probe ---

    /** ヘッダと長さだけを読む。フルデコードはしない。 */
    AssetMeta Probe(const std::string& _path);
    AssetMeta Probe(std::span<const uint8_t> _bytes);

    // --- Static / Streaming ---

    /** Probe して LoadPolicy で決め、Static ならフルデコード、Streaming なら元データだけを持つ。 */
    LoadedAsset LoadAsset(const std::string& _path);
    LoadedAsset LoadAsset(std::vector<uint8_t> _bytes);

} // namespace Audio::Internal::Loader

#endif // Loader_HPP_
