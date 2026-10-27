#include "Math/Easing.hpp"

namespace Ease {
    namespace In {
        // ===== Quadratic Easing (x^2) =====
        Vector2 Quad(Vector2 _start, Vector2 _end, float _t) {
            return _start + (_end - _start) * _t * _t;
        }

        Vector3 Quad(const Vector3& _start, const Vector3& _end, float _t) {
            return _start + (_end - _start) * _t * _t;
        }

        // ===== Cubic Easing (x^3) =====
        Vector2 Cubic(Vector2 _start, Vector2 _end, float _t) {
            return _start + (_end - _start) * _t * _t * _t;
        }

        Vector3 Cubic(const Vector3& _start, const Vector3& _end, float _t) {
            return _start + (_end - _start) * _t * _t * _t;
        }
    }

    namespace Out {
        // ===== Quadratic Easing =====
        Vector2 Quad(Vector2 _start, Vector2 _end, float _t) {
            return _start + (_end - _start) * (1.0f - (1.0f - _t) * (1.0f - _t));
        }

        Vector3 Quad(const Vector3& _start, const Vector3& _end, float _t) {
            return _start + (_end - _start) * (1.0f - (1.0f - _t) * (1.0f - _t));
        }

        // ===== Cubic Easing =====
        Vector2 Cubic(Vector2 _start, Vector2 _end, float _t) {
            return _start + (_end - _start) * (1.0f - (1.0f - _t) * (1.0f - _t) * (1.0f - _t));
        }

        Vector3 Cubic(const Vector3& _start, const Vector3& _end, float _t) {
            return _start + (_end - _start) * (1.0f - (1.0f - _t) * (1.0f - _t) * (1.0f - _t));
        }
    }

    namespace InOut {
        // ===== Quadratic Easing =====
        Vector2 Quad(Vector2 _start, Vector2 _end, float _t) {
            float easedT;
            if (_t < 0.5f)
                easedT = 2.0f * _t * _t;
            else
                easedT = 1.0f - (-2.0f * _t + 2.0f) * (-2.0f * _t + 2.0f) / 2.0f;

            return _start + (_end - _start) * easedT;
        }

        Vector3 Quad(const Vector3& _start, const Vector3& _end, float _t) {
            float easedT;
            if (_t < 0.5f)
                easedT = 2.0f * _t * _t;
            else
                easedT = 1.0f - (-2.0f * _t + 2.0f) * (-2.0f * _t + 2.0f) / 2.0f;

            return _start + (_end - _start) * easedT;
        }

        // ===== Cubic Easing =====
        Vector2 Cubic(Vector2 _start, Vector2 _end, float _t) {
            float easedT;
            if (_t < 0.5f)
                easedT = 4.0f * _t * _t * _t;
            else
                easedT = 1.0f - (-2.0f * _t + 2.0f) * (-2.0f * _t + 2.0f) * (-2.0f * _t + 2.0f) / 2.0f;

            return _start + (_end - _start) * easedT;
        }

        Vector3 Cubic(const Vector3& _start, const Vector3& _end, float _t) {
            float easedT;
            if (_t < 0.5f)
                easedT = 4.0f * _t * _t * _t;
            else
                easedT = 1.0f - (-2.0f * _t + 2.0f) * (-2.0f * _t + 2.0f) * (-2.0f * _t + 2.0f) / 2.0f;

            return _start + (_end - _start) * easedT;
        }
    }
}
