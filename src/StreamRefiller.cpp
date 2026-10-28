#include "StreamRefiller.hpp"

#include <chrono>

namespace Audio::Internal {

    namespace {
        // 巡回の間隔。リングバッファ 2 秒・低水位 0.5 秒に対して十分短い
        constexpr auto kInterval = std::chrono::milliseconds(10);
    }

    StreamRefiller::~StreamRefiller() {
        Shutdown();
    }

    void StreamRefiller::Add(const std::shared_ptr<StreamingVoice>& _voice) {
        {
            std::scoped_lock lock(mutex_);
            voices_.push_back(_voice);
            if (!running_) {
                running_ = true;
                worker_  = std::thread([this] { Run(); });
            }
        }
        cv_.notify_one();
    }

    void StreamRefiller::Shutdown() {
        {
            std::scoped_lock lock(mutex_);
            if (!running_) return;
            running_ = false;
        }
        cv_.notify_one();
        if (worker_.joinable()) worker_.join();
        std::scoped_lock lock(mutex_);
        voices_.clear();
    }

    void StreamRefiller::Run() {
        std::vector<std::shared_ptr<StreamingVoice>> current;
        while (true) {
            {
                std::unique_lock lock(mutex_);
                cv_.wait_for(lock, kInterval);
                if (!running_) return;
                // 手放された再生を外し、残りをロックの外で補充するために取り出す
                std::erase_if(voices_, [](const std::weak_ptr<StreamingVoice>& _voice) { return _voice.expired(); });
                for (const auto& voice : voices_) {
                    if (auto alive = voice.lock()) current.push_back(std::move(alive));
                }
            }
            for (const auto& voice : current) {
                if (voice->NeedsRefill()) voice->Refill();
            }
            // ここで最後の参照が消えた再生は、このスレッドでデコーダごと破棄される（オーディオスレッドではない）
            current.clear();
        }
    }

} // namespace Audio::Internal
