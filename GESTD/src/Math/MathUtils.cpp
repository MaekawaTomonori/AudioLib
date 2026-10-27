#include "Math/MathUtils.hpp"

#include <cassert>
#include <random>
#include <vector>

#include "Math/Transform.hpp"
#include "Pattern/Singleton.hpp"
#include "Random/RandomEngine.hpp"

Matrix3x3 MathUtils::Matrix::MakeIdentity3x3() {
    return Matrix3x3 {
        1,0,0,
        0,1,0,
        0,0,1
    };
}

Matrix4x4 MathUtils::Matrix::MakeIdentity() {
    return Matrix4x4{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
}
Matrix4x4 MathUtils::Matrix::MakeTranslateMatrix(const Vector3& _velocity) {
    return Matrix4x4 {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        _velocity.x, _velocity.y, _velocity.z, 1
    };
}

Matrix4x4 MathUtils::Matrix::MakeScaleMatrix(const Vector3& _scale) {
    return Matrix4x4 {
        _scale.x, 0, 0, 0,
        0, _scale.y, 0, 0,
        0, 0, _scale.z, 0,
        0, 0, 0, 1
    };
}

Vector3 MathUtils::Matrix::Transform(const Vector3& _vector, const Matrix4x4& _matrix) {
    Vector3 v = {
        _vector.x * _matrix.matrix[0][0] + _vector.y * _matrix.matrix[1][0] + _vector.z * _matrix.matrix[2][0] + 1 * _matrix.matrix[3][0],
        _vector.x * _matrix.matrix[0][1] + _vector.y * _matrix.matrix[1][1] + _vector.z * _matrix.matrix[2][1] + 1 * _matrix.matrix[3][1],
        _vector.x * _matrix.matrix[0][2] + _vector.y * _matrix.matrix[1][2] + _vector.z * _matrix.matrix[2][2] + 1 * _matrix.matrix[3][2]
    };
    const float w = _vector.x * _matrix.matrix[0][3] + _vector.y * _matrix.matrix[1][3] + _vector.z * _matrix.matrix[2][3] + 1 * _matrix.matrix[3][3];
    assert(w != 0);
    if (w != 1){
        v.x /= w;
        v.y /= w;
        v.z /= w;
    }
    return v;
}

Vector4 MathUtils::Matrix::Transform(const Vector4& _vector, const Matrix4x4& _matrix) {
    Vector4 v = {
        _vector.x * _matrix.matrix[0][0] + _vector.y * _matrix.matrix[1][0] + _vector.z * _matrix.matrix[2][0] + _vector.w * _matrix.matrix[3][0],
        _vector.x * _matrix.matrix[0][1] + _vector.y * _matrix.matrix[1][1] + _vector.z * _matrix.matrix[2][1] + _vector.w * _matrix.matrix[3][1],
        _vector.x * _matrix.matrix[0][2] + _vector.y * _matrix.matrix[1][2] + _vector.z * _matrix.matrix[2][2] + _vector.w * _matrix.matrix[3][2],
        _vector.x * _matrix.matrix[0][3] + _vector.y * _matrix.matrix[1][3] + _vector.z * _matrix.matrix[2][3] + _vector.w * _matrix.matrix[3][3]
    };
    return v;
}

Matrix4x4 MathUtils::Matrix::MakeRotateX(const float _rad) {
    return {
        1, 0, 0, 0,
        0, std::cosf(_rad), std::sinf(_rad), 0,
        0, -std::sinf(_rad), std::cosf(_rad), 0,
        0, 0, 0, 1
    };
}

Matrix4x4 MathUtils::Matrix::MakeRotateY(float _rad) {
    return Matrix4x4 {
        std::cosf(_rad), 0, -std::sinf(_rad), 0,
        0, 1, 0, 0,
        std::sinf(_rad), 0, std::cosf(_rad), 0,
        0, 0, 0, 1
    };
}

Matrix4x4 MathUtils::Matrix::MakeRotateZ(const float _rad) {
    return Matrix4x4 {
        std::cosf(_rad), std::sinf(_rad), 0, 0,
        -std::sinf(_rad), std::cosf(_rad), 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
}

Matrix4x4 MathUtils::Matrix::MakeRotate(const Quaternion& _rotate) {
    float x2 = _rotate.x * _rotate.x;
    float y2 = _rotate.y * _rotate.y;
    float z2 = _rotate.z * _rotate.z;
    float xy = _rotate.x * _rotate.y;
    float xz = _rotate.x * _rotate.z;
    float xw = _rotate.x * _rotate.w;
    float yz = _rotate.y * _rotate.z;
    float yw = _rotate.y * _rotate.w;
    float zw = _rotate.z * _rotate.w;
    return {
        1.f - 2.f * (y2 + z2), 2.f * (xy + zw), 2.f * (xz - yw), 0.f,
        2.f * (xy - zw), 1.f - 2.f * (x2 + z2), 2.f * (yz + xw), 0.f,
        2.f * (xz + yw), 2.f * (yz - xw), 1.f - 2.f * (x2 + y2), 0.f,
        0.f, 0.f, 0.f, 1.f
    };
}

Matrix4x4 MathUtils::Matrix::MakeAffineMatrix(const ::Transform& _transform) {
    return std::holds_alternative<Vector3>(_transform.rotate) ? MakeAffineMatrix(_transform.scale, std::get<Vector3>(_transform.rotate), _transform.translate) : MakeAffineMatrix(_transform.scale, std::get<Quaternion>(_transform.rotate), _transform.translate);
}

Matrix4x4 MathUtils::Matrix::MakeAffineMatrix(const Vector3& _scale, const Vector3& _rotate, const Vector3& _translate) {
    Matrix4x4 scaleMat = MakeScaleMatrix(_scale);
    Matrix4x4 rotateMatX = MakeRotateX(_rotate.x);
    Matrix4x4 rotateMatY = MakeRotateY(_rotate.y);
    Matrix4x4 rotateMatZ = MakeRotateZ(_rotate.z);
    Matrix4x4 rotateMat = rotateMatX * rotateMatY * rotateMatZ;
    Matrix4x4 translateMat = MakeTranslateMatrix(_translate);
    return scaleMat * rotateMat * translateMat;
}

Matrix4x4 MathUtils::Matrix::MakeAffineMatrix(const Vector3& _scale, const Quaternion& _rotate, const Vector3& _translate) {
    Matrix4x4 scaleMat = MakeScaleMatrix(_scale);
    Matrix4x4 rotateMat = MakeRotate(_rotate);
    Matrix4x4 translateMat = MakeTranslateMatrix(_translate);
    return scaleMat * rotateMat * translateMat;
}

Matrix4x4 MathUtils::Matrix::MakeAffineMatrix(const Matrix4x4& _scale, const Matrix4x4& _rotate,
    const Matrix4x4& _translate) {
    return _scale * _rotate * _translate;
}

Matrix4x4 MathUtils::Matrix::MakeOrthogonalMatrix(float _left, float _right, float _top, float _bottom, float _znear, float _zfar) {
    return Matrix4x4 {
        2 / (_right - _left), 0, 0, 0,
        0, 2 / (_top - _bottom), 0, 0,
        0, 0, 1 / (_zfar - _znear), 0,
        (_left + _right) / (_left - _right), (_top + _bottom) / (_bottom - _top), _znear / (_znear - _zfar), 1
    };
}

Matrix4x4 MathUtils::Matrix::MakePerspectiveFovMatrix(float _fovY, float _aspectRatio, float _nearClip, float _farClip) {
    float halfFovY = tanf(_fovY * 0.5f);
    Vector3 scale = { (1.0f/halfFovY) / _aspectRatio, 1.0f / halfFovY, _farClip / (_farClip - _nearClip)};
    float z = -(_nearClip * _farClip) / (_farClip - _nearClip);

    return Matrix4x4{
        scale.x, 0, 0, 0,
        0, scale.y, 0, 0,
        0, 0, scale.z, 1,
        0, 0, z, 0
    };
}

Matrix4x4 MathUtils::Matrix::MakeViewportMatrix(float _left, float _right, float _top, float _bottom, float _depthMax,
                                                float _depthMin) {
    return Matrix4x4 {
        (_right - _left) / 2, 0, 0, 0,
        0, -(_top - _bottom) / 2, 0, 0,
        0, 0, _depthMax - _depthMin, 0,
        _left + (_right - _left) / 2, _bottom + (_top - _bottom) / 2, _depthMin, 1
    };
}

float MathUtils::Random(float _min, float _max) {
    return Singleton<RandomEngine>::GetInstance()->Get(_min, _max);
}

Vector3 MathUtils::Random(const Vector3& _min, const Vector3& _max) {
    return {
        Random(_min.x, _max.x),
        Random(_min.y, _max.y),
        Random(_min.z, _max.z)
    };
}

float MathUtils::Deg2Rad(float _degree) {
    return _degree * (F_PI / 180.f);
}

float MathUtils::Lerp(const float& _a, const float& _b, const float _t) {
    return _a + (_b - _a) * _t;
}

Vector2 MathUtils::Lerp(const Vector2& _a, const Vector2& _b, const float _t) {
    return Vector2{
        Lerp(_a.x, _b.x, _t),
        Lerp(_a.y, _b.y, _t)
    };
}

Vector3 MathUtils::Lerp(const Vector3& _a, const Vector3& _b, const float _t) {
    return Vector3{
        Lerp(_a.x, _b.x, _t),
        Lerp(_a.y, _b.y, _t),
        Lerp(_a.z, _b.z, _t)
    };
}

Vector4 MathUtils::Lerp(const Vector4& _a, const Vector4& _b, const float _t) {
    return Vector4{
        Lerp(_a.x, _b.x, _t),
        Lerp(_a.y, _b.y, _t),
        Lerp(_a.z, _b.z, _t),
        Lerp(_a.w, _b.w, _t)
    };
}

Quaternion MathUtils::Slerp(const Quaternion& _a, const Quaternion& _b, const float _t) {
    float dot = _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;
    float epsilon = 1e-6f;

    Quaternion b_temp = _b;
    if (dot < 0){
        dot = -dot;
        b_temp = { -_b.x, -_b.y, -_b.z, -_b.w };
    }
    if (1 - dot < epsilon){
        return (_a * (1.0f - _t) + b_temp * _t).Normalize();
    }
    float theta = acosf(dot);
    float sinTheta = sinf(theta);
    float s0 = sinf((1 - _t) * theta) / sinTheta;
    float s1 = sinf(_t * theta) / sinTheta;
    return (_a * s0 + b_temp * s1).Normalize();
}

float MathUtils::Distance(const Vector3& _a, const Vector3& _b) {
    return std::sqrtf(std::powf(_a.x - _b.x, 2) + std::powf(_a.y - _b.y, 2) + std::powf(_a.z - _b.z, 2));
}

Vector3 MathUtils::QuadBezier(const Vector3& _p0, const Vector3& _cp, const Vector3& _p1, float _t) {
    float u = 1.0f - _t;
    return _p0 * (u * u) + _cp * (2.0f * u * _t) + _p1 * (_t * _t);
}

float MathUtils::QuadBezierArcLengthT(const Vector3& _p0, const Vector3& _cp, const Vector3& _p1,
                                       float _s, int _steps) {
    std::vector<float> cumLen(_steps + 1);
    cumLen[0] = 0.0f;
    Vector3 prev = _p0;
    for (int i = 1; i <= _steps; ++i) {
        float   ti  = static_cast<float>(i) / static_cast<float>(_steps);
        Vector3 cur = QuadBezier(_p0, _cp, _p1, ti);
        Vector3 d   = cur - prev;
        cumLen[i]   = cumLen[i - 1] + std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
        prev = cur;
    }

    float totalLen = cumLen[_steps];
    if (totalLen < 1e-6f) return _s;

    float target = std::clamp(_s, 0.0f, 1.0f) * totalLen;
    for (int i = 1; i <= _steps; ++i) {
        if (cumLen[i] >= target) {
            float frac = (cumLen[i] > cumLen[i - 1])
                       ? (target - cumLen[i - 1]) / (cumLen[i] - cumLen[i - 1])
                       : 0.0f;
            return (static_cast<float>(i - 1) + frac) / static_cast<float>(_steps);
        }
    }
    return 1.0f;
}
