#ifndef EASING_HPP_
#define EASING_HPP_

#include "Vector2.hpp"
#include "Vector3.hpp"

namespace Ease {
    // Ease In functions (start slow, accelerate)
    namespace In {
        // Quadratic easing (x^2)
        Vector2 Quad(Vector2 _start, Vector2 _end, float _t);
        Vector3 Quad(const Vector3& _start, const Vector3& _end, float _t);

        // Cubic easing (x^3)
        Vector2 Cubic(Vector2 _start, Vector2 _end, float _t);
        Vector3 Cubic(const Vector3& _start, const Vector3& _end, float _t);
    }

    // Ease Out functions (start fast, decelerate)
    namespace Out {
        // Quadratic easing
        Vector2 Quad(Vector2 _start, Vector2 _end, float _t);
        Vector3 Quad(const Vector3& _start, const Vector3& _end, float _t);

        // Cubic easing
        Vector2 Cubic(Vector2 _start, Vector2 _end, float _t);
        Vector3 Cubic(const Vector3& _start, const Vector3& _end, float _t);
    }

    // Ease InOut functions (accelerate then decelerate)
    namespace InOut {
        // Quadratic easing
        Vector2 Quad(Vector2 _start, Vector2 _end, float _t);
        Vector3 Quad(const Vector3& _start, const Vector3& _end, float _t);

        // Cubic easing
        Vector2 Cubic(Vector2 _start, Vector2 _end, float _t);
        Vector3 Cubic(const Vector3& _start, const Vector3& _end, float _t);
    }
}

#endif // EASING_HPP_
