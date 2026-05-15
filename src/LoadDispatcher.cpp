#include "LoadDispatcher.hpp"

#include <algorithm>

namespace Audio::Internal {

    LoadDispatcher::LoadDispatcher(const size_t _threadCount)
        : running_(true)
    {
        const size_t count = std::max<size_t>(1, _threadCount);
        workers_.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this] { return !tasks_.empty() || !running_; });
                        if (!running_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    LoadDispatcher::~LoadDispatcher() {
        Shutdown();
    }

    void LoadDispatcher::Submit(std::function<void()> _task) {
        {
            std::lock_guard lock(mutex_);
            if (!running_) return;
            tasks_.push(std::move(_task));
        }
        cv_.notify_one();
    }

    void LoadDispatcher::Shutdown() {
        {
            std::lock_guard lock(mutex_);
            if (!running_) return;
            running_ = false;
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        workers_.clear();
    }

} // namespace Audio::Internal
