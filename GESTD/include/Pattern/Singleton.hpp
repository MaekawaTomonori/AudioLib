#ifndef Singleton_HPP_
#define Singleton_HPP_
#include <mutex>
#include <cassert>
#include <memory>

#include "ReferencePtr.hpp"

/** @brief シングルトンファイナライザークラス
 ** シングルトンインスタンスの破棄を管理
 **/
class SingletonFinalizer{
    public:
    using Finalizer = void(*)();

    /** @brief ファイナライザーを追加
     ** @param _finalizer 終了処理関数
     **/
    static void AddFinalizer(Finalizer _finalizer);

    /** @brief 全シングルトンを破棄
     **/
    static void Finalize();
};

/** @brief シングルトンパターンテンプレートクラス
 ** スレッドセーフな単一インスタンス管理を提供
 **/
template <typename T>
class Singleton {
    static std::unique_ptr<T> instance_;
    static std::once_flag flag_;

public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    /** @brief シングルトンインスタンスを取得
     ** @return インスタンスへのポインタ
     **/
    static GESTD::ReferencePtr<T> GetInstance() {
        std::call_once(flag_, Create);
        assert(instance_);
        return GESTD::ReferencePtr<T>(instance_);
    }

private:
    Singleton() = default;
    ~Singleton() = default;

    static void Create() {
        instance_ = std::make_unique<T>();
        SingletonFinalizer::AddFinalizer(Destroy);
    }
    static void Destroy() {
        instance_.reset();
    }
};

template <typename T> inline std::unique_ptr<T> Singleton<T>::instance_ = nullptr;
template <typename T> inline std::once_flag Singleton<T>::flag_;
#endif // Singleton_HPP_
