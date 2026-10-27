#ifndef LifetimeSentinel_HPP_
#define LifetimeSentinel_HPP_
#include <memory>

namespace GESTD {

    /** @brief 生存期間を通知するセンチネル
     ** 所有者のデストラクタで自動的に無効化される。
     ** ReferencePtr と組み合わせてダングリング検出に使う。
     **/
    class LifetimeSentinel {
        std::shared_ptr<bool> alive_;

    public:
        LifetimeSentinel() : alive_(std::make_shared<bool>(true)) {}
        ~LifetimeSentinel() { if (alive_) *alive_ = false; }

        LifetimeSentinel(const LifetimeSentinel&) = delete;
        LifetimeSentinel& operator=(const LifetimeSentinel&) = delete;
        LifetimeSentinel(LifetimeSentinel&&) = default;
        LifetimeSentinel& operator=(LifetimeSentinel&&) = default;

        std::weak_ptr<bool> GetWeak() const noexcept { return alive_; }
    };

} // namespace GESTD

#endif // LifetimeSentinel_HPP_
