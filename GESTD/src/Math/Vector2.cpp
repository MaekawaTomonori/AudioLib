#include "Math/Vector2.hpp"

#include "Math/MathUtils.hpp"

Vector2 Vector2::Random() {
    return {
        MathUtils::Random(-1.f, 1.f),
        MathUtils::Random(-1.f, 1.f),
    };
}
