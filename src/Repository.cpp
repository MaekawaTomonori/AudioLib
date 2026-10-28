#include "Repository.hpp"

namespace Audio::Internal {

    // 同名が既にあればその id、なければ有効なときだけ新しい id で _store する（呼ぶ側で lock 済み）
    template <class Store>
    uint64_t Repository::RegisterAsset(const std::string& _name, const bool _valid, Store _store) {
        if (const auto it = nameToId_.find(_name); it != nameToId_.end()) {
            return it->second;
        }
        if (!_valid) return 0;
        const uint64_t id = nextId_++;
        nameToId_[_name] = id;
        _store(data_[id]);
        return id;
    }

    uint64_t Repository::Register(std::unique_ptr<Sound> _data, const std::string& _name) {
        std::lock_guard lock(mutex_);
        const bool valid = _data && _data->IsValid();
        return RegisterAsset(_name, valid, [&](Asset& _asset) { _asset.sound = std::move(_data); });
    }

    uint64_t Repository::Register(std::unique_ptr<StreamingAsset> _data, const std::string& _name) {
        std::lock_guard lock(mutex_);
        const bool valid = _data && _data->IsValid();
        return RegisterAsset(_name, valid, [&](Asset& _asset) { _asset.streaming = std::move(_data); });
    }

    std::shared_ptr<const Sound> Repository::Find(const uint64_t _id) const {
        std::lock_guard lock(mutex_);
        const auto it = data_.find(_id);
        return it != data_.end() ? it->second.sound : nullptr;
    }

    std::shared_ptr<const StreamingAsset> Repository::FindStreaming(const uint64_t _id) const {
        std::lock_guard lock(mutex_);
        const auto it = data_.find(_id);
        return it != data_.end() ? it->second.streaming : nullptr;
    }

    bool Repository::Contains(const std::string& _name) const {
        std::lock_guard lock(mutex_);
        return nameToId_.contains(_name);
    }

    uint64_t Repository::FindId(const std::string& _name) const {
        std::lock_guard lock(mutex_);
        const auto it = nameToId_.find(_name);
        return it != nameToId_.end() ? it->second : 0;
    }

    std::string Repository::FindName(const uint64_t _id) const {
        std::lock_guard lock(mutex_);
        for (const auto& [name, id] : nameToId_) {
            if (id == _id) return name;
        }
        return {};
    }

    void Repository::Unregister(const uint64_t _id) {
        std::lock_guard lock(mutex_);
        if (data_.erase(_id) == 0) return;
        std::erase_if(nameToId_, [_id](const auto& _entry) { return _entry.second == _id; });
    }

    void Repository::Clear() {
        std::lock_guard lock(mutex_);
        data_.clear();
        nameToId_.clear();
        nextId_ = 1;
    }

} // namespace Audio::Internal
