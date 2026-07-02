#ifndef VECTOR3_HPP_
#define VECTOR3_HPP_

#include <cmath>

#include "Vector2.hpp"

/** @brief 3Dベクトル構造体
 ** 3D座標と基本的なベクトル演算を提供
 **/
struct Vector3{
    float x = 0.f, y = 0.f, z = 0.f;

    Vector3& operator=(const float _s) {
        x = y = z = _s;
        return *this;
    }

    Vector3 operator+(const Vector3& _v) const {
        return {x + _v.x, y + _v.y, z + _v.z};
    }

	void operator+=(const Vector3& _v) {
		x += _v.x;
		y += _v.y;
		z += _v.z;
	}
	void operator+=(const Vector2& _v) {
		x += _v.x;
		y += _v.y;
		z += 0;
	}

    Vector3 operator-(const Vector3& _v) const {
        return {x - _v.x, y - _v.y, z - _v.z};
    }

    Vector3 operator*(const float _f) const {
        return {x * _f, y * _f, z * _f};
    }

    void operator*=(const float _f) {
        x *= _f;
        y *= _f;
        z *= _f;
    }

    void operator*=(const Vector3& _v) {
        x *= _v.x;
        y *= _v.y;
        z *= _v.z;
    }

	Vector3 operator/(const float& _f) const {
        return {x / _f, y / _f, z / _f};
    }

    Vector3 operator/(const Vector3& _v) const {
        return {x / _v.x, y / _v.y, z / _v.z};
    }

	float Length() const {
		return sqrtf(x * x + y * y + z * z);
	}

	Vector3 Normalize() const {
        const float len = Length();
        if (len <= 0.0000001f) return *this;

		return Vector3{
			x / len,
			y / len,
			z / len
		};
	}

	static Vector3 Random();
};

#endif // VECTOR3_HPP_
