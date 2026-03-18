#ifndef Repository_HPP_
#define Repository_HPP_
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Sound.hpp"

namespace Audio {

    class Repository {
        std::unordered_map<uint64_t, std::unique_ptr<Sound>> data_;
        std::unordered_map<std::string, uint64_t>            nameToId_; // 名前 → id 逆引き
        uint64_t nextId_ = 1;

    public:
        // 名前付きで登録する。同名が既にある場合は既存の id を返す
        uint64_t     Register(std::unique_ptr<Sound> _data, std::string_view _name);

        // id で検索（Mixer が使用）
        const Sound* Find(uint64_t _id) const;

        // 名前で登録済みか確認する
        bool         Contains(std::string_view _name) const;

        // 名前から id を取得する（未登録の場合は 0）
        uint64_t         FindId(std::string_view _name) const;

        // id から名前を取得する（未登録の場合は空文字列）
        std::string_view FindName(uint64_t _id) const;
    };

} // namespace Handle

#endif // Repository_HPP_
