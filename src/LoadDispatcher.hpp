#ifndef LoadDispatcher_HPP_
#define LoadDispatcher_HPP_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Audio::Internal {

    class LoadDispatcher {
        std::vector<std::thread>          workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex                        mutex_;
        std::condition_variable           cv_;
        bool                              running_ = false;

    public:
        explicit LoadDispatcher(size_t _threadCount);
        ~LoadDispatcher();

        void Submit(std::function<void()> _task);

        // Wait for all in-flight tasks, then stop workers.
        void Shutdown();
    };

} // namespace Audio::Internal

#endif // LoadDispatcher_HPP_
