#ifndef WeakPtr_HPP_
#define WeakPtr_HPP_
#include <cassert>
#include <memory>

#include "../src/LifetimeSentinel.hpp"

/** @brief GameEngine Standard
 */
namespace GESTD {

    template<typename T>
    class ReferencePtr {
        T* ptr_ = nullptr;
        std::weak_ptr<bool> alive_;
        bool hasSentinel_ = false;

        bool IsValid() const noexcept {
            if (ptr_ == nullptr) return false;
            if (!hasSentinel_) return true;
            const auto sp = alive_.lock();
            return sp && *sp;
        }

        void Validate() const noexcept {
            assert(IsValid() && "ReferencePtr: null or dangling dereference");
        }

    public:
        ReferencePtr() = default;
        ReferencePtr(std::nullptr_t) noexcept {}

        /** センチネルなし（nullptr チェックのみ） */
        explicit ReferencePtr(T* _ptr) noexcept : ptr_(_ptr) {}
        explicit ReferencePtr(const std::unique_ptr<T>& _ptr) noexcept : ptr_(_ptr.get()) {}

        /** センチネルあり（ダングリング検出つき） */
        ReferencePtr(T* _ptr, const LifetimeSentinel& _sentinel) noexcept
            : ptr_(_ptr), alive_(_sentinel.GetWeak()), hasSentinel_(true) {}

        ReferencePtr& operator=(std::nullptr_t) noexcept {
            ptr_ = nullptr; alive_.reset(); hasSentinel_ = false; return *this;
        }
        ReferencePtr& operator=(T* _ptr) noexcept {
            ptr_ = _ptr; alive_.reset(); hasSentinel_ = false; return *this;
        }
        ReferencePtr& operator=(const std::unique_ptr<T>& _ptr) noexcept {
            ptr_ = _ptr.get(); alive_.reset(); hasSentinel_ = false; return *this;
        }

        T* operator->() const noexcept { Validate(); return ptr_; }
        T& operator*()  const noexcept { Validate(); return *ptr_; }

        // @Deprecated
        operator T*() const noexcept { return IsValid() ? ptr_ : nullptr; }

        explicit operator bool() const noexcept { return IsValid(); }

        void Reset() noexcept { ptr_ = nullptr; alive_.reset(); hasSentinel_ = false; }

    }; // class ReferencePtr

} // namespace GESTD

#endif // WeakPtr_HPP_
