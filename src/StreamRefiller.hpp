#ifndef StreamRefiller_HPP_
#define StreamRefiller_HPP_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "StreamingVoice.hpp"

namespace Audio::Internal {

    /** StreamingVoice のリングバッファを補充するワーカースレッド 1 本。
     *  デコード・file IO・デコーダの生成と破棄はここで行い、オーディオスレッドには持ち込まない。
     *  再生は weak_ptr で持つので、再生（Mixer）が手放したものは次の巡回で外れる。
     */
    class StreamRefiller {
        std::thread                                worker_;
        std::mutex                                 mutex_;
        std::condition_variable                    cv_;
        std::vector<std::weak_ptr<StreamingVoice>> voices_;
        bool                                       running_ = false;

    public:
        StreamRefiller() = default;
        ~StreamRefiller();
        StreamRefiller(const StreamRefiller&)            = delete;
        StreamRefiller& operator=(const StreamRefiller&) = delete;

        /** 補充の対象に加え、すぐに最初の準備をさせる。初回にワーカーを起こす。 */
        void Add(const std::shared_ptr<StreamingVoice>& _voice);
        /** ワーカーを止めて待つ。対象はすべて手放す。 */
        void Shutdown();

    private:
        // --- Worker Loop ---
        void Run();
    };

} // namespace Audio::Internal

#endif // StreamRefiller_HPP_
