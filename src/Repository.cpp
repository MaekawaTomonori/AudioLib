#include "Repository.hpp"

namespace Audio {

    uint64_t Repository::Register(std::unique_ptr<Sound> _data, std::string_view _name) {
        // 同名が既に登録済みなら既存の id を返す
        if (const auto it = nameToId_.find(std::string(_name)); it != nameToId_.end()) {
            return it->second;
        }

        if (!_data || !_data->IsValid()) return 0;

        const uint64_t id = nextId_++;
        nameToId_[std::string(_name)] = id;
        data_[id] = std::move(_data);
        return id;
    }

    const Sound* Repository::Find(uint64_t _id) const {
        const auto it = data_.find(_id);
        return it != data_.end() ? it->second.get() : nullptr;
    }

    bool Repository::Contains(std::string_view _name) const {
        return nameToId_.contains(std::string(_name));
    }

    uint64_t Repository::FindId(std::string_view _name) const {
        const auto it = nameToId_.find(std::string(_name));
        return it != nameToId_.end() ? it->second : 0;
    }

    std::string_view Repository::FindName(uint64_t _id) const {
        for (const auto& [name, id] : nameToId_) {
            if (id == _id) return name;
        }
        return {};
    }

} // namespace Audio
