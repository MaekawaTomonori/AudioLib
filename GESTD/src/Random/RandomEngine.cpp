#include "Random/RandomEngine.hpp"

void RandomEngine::Initialize() {
    seed_ = seedGen_();
    engine_.seed(seed_);
}

int32_t RandomEngine::Get(int32_t _min, int32_t _max) {
    std::uniform_int_distribution<int32_t> dist(_min, _max);
    return dist(engine_);
}

float RandomEngine::Get(float _min, float _max) {
    std::uniform_real_distribution<float> dist(_min, _max);
    return dist(engine_);
}
