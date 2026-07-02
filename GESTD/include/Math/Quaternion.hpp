#ifndef Quaternion_HPP_
#define Quaternion_HPP_
#include <cmath>

/** @brief クォータニオン構造体
 ** 3D回転を表現し、ジンバルロックを回避
 **/
struct Quaternion{
    float x, y, z, w;

    Quaternion operator+(const Quaternion& _q) const {
        return { x + _q.x, y + _q.y, z + _q.z, w + _q.w };
    }

    Quaternion operator*(const Quaternion& _q) const {
        return {
            w * _q.x + x * _q.w + y * _q.z - z * _q.y,
            w * _q.y - x * _q.z + y * _q.w + z * _q.x,
            w * _q.z + x * _q.y - y * _q.x + z * _q.w,
            w * _q.w - x * _q.x - y * _q.y - z * _q.z
        };
    }

    Quaternion operator*(const float _s) const {
        return { x * _s, y * _s, z * _s, w * _s };
    }

    Quaternion operator/(const float _s) const {
        return { x / _s, y / _s, z / _s, w / _s };
    }

    static Quaternion Identity() {
        return { 0, 0, 0, 1 };
    }

    Quaternion Conjugate() const {
        return { -x, -y, -z, w };
    }

    float Norm() const {
        return sqrtf(x * x + y * y + z * z + w * w);
    }

    Quaternion Normalize() const {
        return *this / Norm();
    }

    Quaternion Inverse() const {
        return Conjugate() / (x * x + y * y + z * z + w * w);
    }
};
#endif // Quaternion_HPP_
