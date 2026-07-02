#include "Math/Vector3.hpp"

#include "Math/MathUtils.hpp"

Vector3 Vector3::Random() {
    return {
        MathUtils::Random(-1.f, 1.f),
        MathUtils::Random(-1.f, 1.f),
        MathUtils::Random(-1.f, 1.f)
    };
}
