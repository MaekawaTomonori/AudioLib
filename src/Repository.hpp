#ifndef Repository_HPP_
#define Repository_HPP_
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Sound.hpp"
#include "StreamingAsset.hpp"

namespace Audio::Internal {

    class Repository {
        // 1 つの id に、全 PCM の Sound か StreamingAsset のどちらか一方
        struct Asset {
            std::shared_ptr<Sound>          sound;
            std::shared_ptr<StreamingAsset> streaming;
        };

        mutable std::mutex                  mutex_;
        std::unordered_map<uint64_t, Asset> data_;
        std::unordered_map<std::string, uint64_t>            nameToId_;
        uint64_t nextId_ = 1;

    public:
        // 名前付きで登録する。同名が既にある場合は既存の id を返す
        uint64_t     Register(std::unique_ptr<Sound> _data, const std::string& _name);
        uint64_t     Register(std::unique_ptr<StreamingAsset> _data, const std::string& _name);

        // id で検索（Mixer が使用）
        std::shared_ptr<const Sound> Find(uint64_t _id) const;
        // id で Streaming のアセットを検索する。Static の id では nullptr
        std::shared_ptr<const StreamingAsset> FindStreaming(uint64_t _id) const;

        // 名前で登録済みか確認する
        bool         Contains(const std::string& _name) const;

        // 名前から id を取得する（未登録の場合は 0）
        uint64_t     FindId(const std::string& _name) const;

        // id から名前を取得する（未登録の場合は空文字列）
        std::string  FindName(uint64_t _id) const;

        // id の登録を外す。再生中のインスタンスは shared_ptr で実体を持ち続ける
        void         Unregister(uint64_t _id);

        // 全リソースを解放し、初期状態に戻す
        void         Clear();

    private:
        template <class Store>
        uint64_t RegisterAsset(const std::string& _name, bool _valid, Store _store);
    };

} // namespace Audio::Internal

#endif // Repository_HPP_
