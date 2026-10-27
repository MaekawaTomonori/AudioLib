#ifndef MATH_UTILS_HPP_
#define MATH_UTILS_HPP_

#include <cmath>

#include "Matrix.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"
#include "Vector4.hpp"

//struct Transform;

namespace MathUtils{
	constexpr float F_PI = 3.14159265358979323846264338327950288f;
	constexpr double PI  = 3.14159265358979323846264338327950288;

    float Random(float _min, float _max);
    Vector3 Random(const Vector3& _min, const Vector3& _max);
    
    float Deg2Rad(float _degree);

    float Lerp(const float& _a, const float& _b, float _t);

    Vector2 Lerp(const Vector2& _a, const Vector2& _b, float _t);
    Vector3 Lerp(const Vector3& _a, const Vector3& _b, float _t);
    Vector4 Lerp(const Vector4& _a, const Vector4& _b, float _t);
    Quaternion Slerp(const Quaternion& _a, const Quaternion& _b, float _t);

    template<typename Type>
    Type Factorial(Type _n) {
        if (_n <= 0)return 1;

        return static_cast<Type>(_n * Factorial(_n - 1));
    }

    template <typename Type>
    Type Permutation(Type _n, Type _r) {
        return static_cast<Type>(Factorial(_n) / Factorial(_n - _r));
    }

    float Distance(const Vector3& _a, const Vector3& _b);
    //Vector3 TransformNormal(const Vector3& v, const Transform& t);

    /** @brief 2次ベジェ曲線をパラメータ t で評価する **/
    Vector3 QuadBezier(const Vector3& _p0, const Vector3& _cp, const Vector3& _p1, float _t);

    /** @brief 弧長再パラメータ化：距離割合 _s ∈ [0,1] に対応するベジェパラメータ t を返す
     ** @param _steps 弧長テーブルの分割数（精度）。デフォルト 32
     **/
    float QuadBezierArcLengthT(const Vector3& _p0, const Vector3& _cp, const Vector3& _p1, float _s, int _steps = 32);

    namespace Matrix {
        Matrix3x3 MakeIdentity3x3();

        Matrix4x4 MakeIdentity();
        Matrix4x4 MakeTranslateMatrix(const Vector3& _velocity);
        Matrix4x4 MakeScaleMatrix(const Vector3& _scale);
        Vector3 Transform(const Vector3& _vector, const Matrix4x4& _matrix);
        Vector4 Transform(const Vector4& _vector, const Matrix4x4& _matrix);

        Matrix4x4 MakeRotateX(float _rad);
        Matrix4x4 MakeRotateY(float _rad);
        Matrix4x4 MakeRotateZ(float _rad);
        Matrix4x4 MakeRotate(const Quaternion& _rotate);
        Matrix4x4 MakeAffineMatrix(const ::Transform& _transform);
        Matrix4x4 MakeAffineMatrix(const Vector3& _scale, const Vector3& _rotate, const Vector3& _translate);
        Matrix4x4 MakeAffineMatrix(const Vector3& _scale, const Quaternion& _rotate, const Vector3& _translate);
        Matrix4x4 MakeAffineMatrix(const Matrix4x4& _scale, const Matrix4x4& _rotate, const Matrix4x4& _translate);

        Matrix4x4 MakeOrthogonalMatrix(float _left, float _right, float _top, float _bottom, float _znear, float _zfar);
        Matrix4x4 MakePerspectiveFovMatrix(float _fovY, float _aspectRatio, float _nearClip, float _farClip);
        Matrix4x4 MakeViewportMatrix(float _width, float _height, float _top, float _bottom, float _depthMax, float _depthMin);
    }
};

#endif // MATH_UTILS_HPP_
