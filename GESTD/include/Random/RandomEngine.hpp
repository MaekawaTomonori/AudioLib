#ifndef RandomEngine_HPP_
#define RandomEngine_HPP_
#include <random>

class RandomEngine {
    std::random_device seedGen_;
    uint32_t seed_ = 0;
    std::mt19937 engine_;

public:
    void Initialize();

    int32_t Get(int32_t _min, int32_t _max);

    float Get(float _min, float _max);

private:

}; // class RandomEngine

#endif // RandomEngine_HPP_
