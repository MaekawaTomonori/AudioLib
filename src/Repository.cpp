#include "Repository.hpp"

namespace Audio::Internal {

    uint64_t Repository::Register(std::unique_ptr<Sound> _data, const std::string& _name) {
        std::lock_guard lock(mutex_);
        if (const auto it = nameToId_.find(_name); it != nameToId_.end()) {
            return it->second;
        }
        if (!_data || !_data->IsValid()) return 0;
        const uint64_t id = nextId_++;
        nameToId_[_name] = id;
        data_[id] = std::move(_data);
        return id;
    }

    const Sound* Repository::Find(const uint64_t _id) const {
        std::lock_guard lock(mutex_);
        const auto it = data_.find(_id);
        return it != data_.end() ? it->second.get() : nullptr;
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

    void Repository::Clear() {
        std::lock_guard lock(mutex_);
        data_.clear();
        nameToId_.clear();
        nextId_ = 1;
    }

} // namespace Audio::Internal
