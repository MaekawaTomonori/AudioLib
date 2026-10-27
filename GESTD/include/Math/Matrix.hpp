#ifndef MATRIX_HPP_
#define MATRIX_HPP_

#include "Vector3.hpp"

/** @brief 3x3行列構造体
 **/
struct Matrix3x3{
    float matrix[3][3];
};

/** @brief 4x4行列構造体
 ** 3D変換と投影演算を提供
 **/
struct Matrix4x4{
    float matrix[4][4];

    Matrix4x4 operator+(const Matrix4x4& _other) const {
        return Matrix4x4 {
            matrix[0][0] + _other.matrix[0][0],
            matrix[0][1] + _other.matrix[0][1],
            matrix[0][2] + _other.matrix[0][2],
            matrix[0][3] + _other.matrix[0][3],
            matrix[1][0] + _other.matrix[1][0],
            matrix[1][1] + _other.matrix[1][1],
            matrix[1][2] + _other.matrix[1][2],
            matrix[1][3] + _other.matrix[1][3],
            matrix[2][0] + _other.matrix[2][0],
            matrix[2][1] + _other.matrix[2][1],
            matrix[2][2] + _other.matrix[2][2],
            matrix[2][3] + _other.matrix[2][3],
            matrix[3][0] + _other.matrix[3][0],
            matrix[3][1] + _other.matrix[3][1],
            matrix[3][2] + _other.matrix[3][2],
            matrix[3][3] + _other.matrix[3][3]
        };
    }
    Matrix4x4 operator-(const Matrix4x4& _other) const {
        return Matrix4x4 {
            matrix[0][0] - _other.matrix[0][0],
            matrix[0][1] - _other.matrix[0][1],
            matrix[0][2] - _other.matrix[0][2],
            matrix[0][3] - _other.matrix[0][3],
            matrix[1][0] - _other.matrix[1][0],
            matrix[1][1] - _other.matrix[1][1],
            matrix[1][2] - _other.matrix[1][2],
            matrix[1][3] - _other.matrix[1][3],
            matrix[2][0] - _other.matrix[2][0],
            matrix[2][1] - _other.matrix[2][1],
            matrix[2][2] - _other.matrix[2][2],
            matrix[2][3] - _other.matrix[2][3],
            matrix[3][0] - _other.matrix[3][0],
            matrix[3][1] - _other.matrix[3][1],
            matrix[3][2] - _other.matrix[3][2],
            matrix[3][3] - _other.matrix[3][3]
        };
    }
    Matrix4x4 operator*(const float& _other) const {
        return Matrix4x4 {
            matrix[0][0] * _other,
            matrix[0][1] * _other,
            matrix[0][2] * _other,
            matrix[0][3] * _other,
            matrix[1][0] * _other,
            matrix[1][1] * _other,
            matrix[1][2] * _other,
            matrix[1][3] * _other,
            matrix[2][0] * _other,
            matrix[2][1] * _other,
            matrix[2][2] * _other,
            matrix[2][3] * _other,
            matrix[3][0] * _other,
            matrix[3][1] * _other,
            matrix[3][2] * _other,
            matrix[3][3] * _other
        };
    }
    Matrix4x4 operator*(const Matrix4x4& _other) const {
        Matrix4x4 m {};
        for (int i = 0; i < 4; ++i){
            for (int j = 0; j < 4; ++j){
                for (int k = 0; k < 4; ++k){
                    m.matrix[i][j] += matrix[i][k] * _other.matrix[k][j];
                }
            }
        }
        return m;
    }
    Matrix4x4 operator/(const float& _other) const {
        return Matrix4x4 {
            matrix[0][0] / _other,
            matrix[0][1] / _other,
            matrix[0][2] / _other,
            matrix[0][3] / _other,
            matrix[1][0] / _other,
            matrix[1][1] / _other,
            matrix[1][2] / _other,
            matrix[1][3] / _other,
            matrix[2][0] / _other,
            matrix[2][1] / _other,
            matrix[2][2] / _other,
            matrix[2][3] / _other,
            matrix[3][0] / _other,
            matrix[3][1] / _other,
            matrix[3][2] / _other,
            matrix[3][3] / _other
        };
    }
    Matrix4x4 Inverse() const {
        float a = matrix[0][0] * matrix[1][1] * matrix[2][2] * matrix[3][3] + matrix[0][0] * matrix[1][2] * matrix[2][3] * matrix[3][1] + matrix[0][0] * matrix[1][3] * matrix[2][1] * matrix[3][2] - matrix[0][0] * matrix[1][3] * matrix[2][2] * matrix[3][1] - matrix[0][0] * matrix[1][2] * matrix[2][1] * matrix[3][3] - matrix[0][0] * matrix[1][1] * matrix[2][3] * matrix[3][2] - matrix[0][1] * matrix[1][0] * matrix[2][2] * matrix[3][3] - matrix[0][2] * matrix[1][0] * matrix[2][3] * matrix[3][1] - matrix[0][3] * matrix[1][0] * matrix[2][1] * matrix[3][2] + matrix[0][3] * matrix[1][0] * matrix[2][2] * matrix[3][1] + matrix[0][2] * matrix[1][0] * matrix[2][1] * matrix[3][3] + matrix[0][1] * matrix[1][0] * matrix[2][3] * matrix[3][2] + matrix[0][1] * matrix[1][2] * matrix[2][0] * matrix[3][3] + matrix[0][2] * matrix[1][3] * matrix[2][0] * matrix[3][1] + matrix[0][3] * matrix[1][1] * matrix[2][0] * matrix[3][2] - matrix[0][3] * matrix[1][2] * matrix[2][0] * matrix[3][1] - matrix[0][2] * matrix[1][1] * matrix[2][0] * matrix[3][3] - matrix[0][1] * matrix[1][3] * matrix[2][0] * matrix[3][2] - matrix[0][1] * matrix[1][2] * matrix[2][3] * matrix[3][0] - matrix[0][2] * matrix[1][3] * matrix[2][1] * matrix[3][0] - matrix[0][3] * matrix[1][1] * matrix[2][2] * matrix[3][0] + matrix[0][3] * matrix[1][2] * matrix[2][1] * matrix[3][0] + matrix[0][2] * matrix[1][1] * matrix[2][3] * matrix[3][0] + matrix[0][1] * matrix[1][3] * matrix[2][2] * matrix[3][0];

        // Check for zero determinant
        if (std::abs(a) < 1e-8f) {
            // Return identity matrix if determinant is zero
            return Matrix4x4{
                1,0,0,0,
                0,1,0,0,
                0,0,1,0,
                0,0,0,1
            };
        }

        return Matrix4x4 {
            (matrix[1][1] * matrix[2][2] * matrix[3][3] + matrix[1][2] * matrix[2][3] * matrix[3][1] + matrix[1][3] * matrix[2][1] * matrix[3][2] - matrix[1][3] * matrix[2][2] * matrix[3][1] - matrix[1][2] * matrix[2][1] * matrix[3][3] - matrix[1][1] * matrix[2][3] * matrix[3][2]),
            (-matrix[0][1] * matrix[2][2] * matrix[3][3] - matrix[0][2] * matrix[2][3] * matrix[3][1] - matrix[0][3] * matrix[2][1] * matrix[3][2] + matrix[0][3] * matrix[2][2] * matrix[3][1] + matrix[0][2] * matrix[2][1] * matrix[3][3] + matrix[0][1] * matrix[2][3] * matrix[3][2]),
            (matrix[0][1] * matrix[1][2] * matrix[3][3] + matrix[0][2] * matrix[1][3] * matrix[3][1] + matrix[0][3] * matrix[1][1] * matrix[3][2] - matrix[0][3] * matrix[1][2] * matrix[3][1] - matrix[0][2] * matrix[1][1] * matrix[3][3] - matrix[0][1] * matrix[1][3] * matrix[3][2]),
            (-matrix[0][1] * matrix[1][2] * matrix[2][3] - matrix[0][2] * matrix[1][3] * matrix[2][1] - matrix[0][3] * matrix[1][1] * matrix[2][2] + matrix[0][3] * matrix[1][2] * matrix[2][1] + matrix[0][2] * matrix[1][1] * matrix[2][3] + matrix[0][1] * matrix[1][3] * matrix[2][2]),

            (-matrix[1][0] * matrix[2][2] * matrix[3][3] - matrix[1][2] * matrix[2][3] * matrix[3][0] - matrix[1][3] * matrix[2][0] * matrix[3][2] + matrix[1][3] * matrix[2][2] * matrix[3][0] + matrix[1][2] * matrix[2][0] * matrix[3][3] + matrix[1][0] * matrix[2][3] * matrix[3][2]),
            (matrix[0][0] * matrix[2][2] * matrix[3][3] + matrix[0][2] * matrix[2][3] * matrix[3][0] + matrix[0][3] * matrix[2][0] * matrix[3][2] - matrix[0][3] * matrix[2][2] * matrix[3][0] - matrix[0][2] * matrix[2][0] * matrix[3][3] - matrix[0][0] * matrix[2][3] * matrix[3][2]),
            (-matrix[0][0] * matrix[1][2] * matrix[3][3] - matrix[0][2] * matrix[1][3] * matrix[3][0] - matrix[0][3] * matrix[1][0] * matrix[3][2] + matrix[0][3] * matrix[1][2] * matrix[3][0] + matrix[0][2] * matrix[1][0] * matrix[3][3] + matrix[0][0] * matrix[1][3] * matrix[3][2]),
            (matrix[0][0] * matrix[1][2] * matrix[2][3] + matrix[0][2] * matrix[1][3] * matrix[2][0] + matrix[0][3] * matrix[1][0] * matrix[2][2] - matrix[0][3] * matrix[1][2] * matrix[2][0] - matrix[0][2] * matrix[1][0] * matrix[2][3] - matrix[0][0] * matrix[1][3] * matrix[2][2]),

            (matrix[1][0] * matrix[2][1] * matrix[3][3] + matrix[1][1] * matrix[2][3] * matrix[3][0] + matrix[1][3] * matrix[2][0] * matrix[3][1] - matrix[1][3] * matrix[2][1] * matrix[3][0] - matrix[1][1] * matrix[2][0] * matrix[3][3] - matrix[1][0] * matrix[2][3] * matrix[3][1]),
            (-matrix[0][0] * matrix[2][1] * matrix[3][3] - matrix[0][1] * matrix[2][3] * matrix[3][0] - matrix[0][3] * matrix[2][0] * matrix[3][1] + matrix[0][3] * matrix[2][1] * matrix[3][0] + matrix[0][1] * matrix[2][0] * matrix[3][3] + matrix[0][0] * matrix[2][3] * matrix[3][1]),
            (matrix[0][0] * matrix[1][1] * matrix[3][3] + matrix[0][1] * matrix[1][3] * matrix[3][0] + matrix[0][3] * matrix[1][0] * matrix[3][1] - matrix[0][3] * matrix[1][1] * matrix[3][0] - matrix[0][1] * matrix[1][0] * matrix[3][3] - matrix[0][0] * matrix[1][3] * matrix[3][1]),
            (-matrix[0][0] * matrix[1][1] * matrix[2][3] - matrix[0][1] * matrix[1][3] * matrix[2][0] - matrix[0][3] * matrix[1][0] * matrix[2][1] + matrix[0][3] * matrix[1][1] * matrix[2][0] + matrix[0][1] * matrix[1][0] * matrix[2][3] + matrix[0][0] * matrix[1][3] * matrix[2][1]),

            (-matrix[1][0] * matrix[2][1] * matrix[3][2] - matrix[1][1] * matrix[2][2] * matrix[3][0] - matrix[1][2] * matrix[2][0] * matrix[3][1] + matrix[1][2] * matrix[2][1] * matrix[3][0] + matrix[1][1] * matrix[2][0] * matrix[3][2] + matrix[1][0] * matrix[2][2] * matrix[3][1]),
            (matrix[0][0] * matrix[2][1] * matrix[3][2] + matrix[0][1] * matrix[2][2] * matrix[3][0] + matrix[0][2] * matrix[2][0] * matrix[3][1] - matrix[0][2] * matrix[2][1] * matrix[3][0] - matrix[0][1] * matrix[2][0] * matrix[3][2] - matrix[0][0] * matrix[2][2] * matrix[3][1]),
            (-matrix[0][0] * matrix[1][1] * matrix[3][2] - matrix[0][1] * matrix[1][2] * matrix[3][0] - matrix[0][2] * matrix[1][0] * matrix[3][1] + matrix[0][2] * matrix[1][1] * matrix[3][0] + matrix[0][1] * matrix[1][0] * matrix[3][2] + matrix[0][0] * matrix[1][2] * matrix[3][1]),
            (matrix[0][0] * matrix[1][1] * matrix[2][2] + matrix[0][1] * matrix[1][2] * matrix[2][0] + matrix[0][2] * matrix[1][0] * matrix[2][1] - matrix[0][2] * matrix[1][1] * matrix[2][0] - matrix[0][1] * matrix[1][0] * matrix[2][2] - matrix[0][0] * matrix[1][2] * matrix[2][1])
        } / a;
    }

    Matrix4x4 Transpose() const {
        Matrix4x4 m {};

        for (int row = 0; row < 4; ++row){
            for (int column = 0; column < 4; ++column){
                m.matrix[row][column] = matrix[column][row];
            }
        }

        return m;
    }

    Vector3 GetTranslate() const {
        return Vector3{
            .x= matrix[3][0],
            .y= matrix[3][1],
            .z= matrix[3][2]
        };
    }
};

#endif // MATRIX_HPP_
