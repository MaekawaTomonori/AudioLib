#include "Pattern/Singleton.hpp"

namespace{
    constexpr size_t MAX_FINALIZER_SIZE = 256;
    size_t finalizerSize = 0;
    std::mutex mutex;

    SingletonFinalizer::Finalizer finalizers[MAX_FINALIZER_SIZE];
}

void SingletonFinalizer::AddFinalizer(Finalizer _finalizer) {
    std::lock_guard<std::mutex> lock(mutex);
    assert(finalizerSize < MAX_FINALIZER_SIZE);
    finalizers[finalizerSize++] = _finalizer;
}

void SingletonFinalizer::Finalize() {
    std::lock_guard<std::mutex> lock(mutex);
    for (int i = static_cast<int>(finalizerSize) - 1; 0 <= i; --i){
        (*finalizers[i])();
    }
    finalizerSize = 0;
}
