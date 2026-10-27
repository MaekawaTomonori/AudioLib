#ifndef VECTOR2_HPP_
#define VECTOR2_HPP_

#include <cmath>

/** @brief 2Dベクトル構造体
 ** 2D座標と基本的なベクトル演算を提供
 **/
struct Vector2{
    float x, y;

    Vector2 operator+(const Vector2& _v) const {
        return { x + _v.x, y + _v.y };
    }

    Vector2 operator-(const Vector2& _v) const {
        return { x - _v.x, y - _v.y };
    }

    Vector2 operator*(const Vector2& _v) const {
		return { x * _v.x, y * _v.y };
    }

    Vector2 operator*(const float _f) const {
        return { x * _f, y * _f };
    }

    Vector2 operator/(const Vector2& _v) const {
        return { x / _v.x, y / _v.y };
    }

    Vector2 operator/(const float _f) const {
        return { x / _f, y / _f };
    }

	void operator+=(const Vector2& _v) {
		x += _v.x;
		y += _v.y;
	}

    bool operator==(const Vector2& _v) const {
        return x == _v.x && y == _v.y;
    }

    float Length() const {
        return sqrtf(x * x + y * y);
    }

    void Normalize() {
        if (Length() <= 0.0000001f)return;
        float length = Length();
        x /= length;
        y /= length;
    }

    static Vector2 Random();
};

#endif // VECTOR2_HPP_
