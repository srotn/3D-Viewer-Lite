#pragma once
#include <vector>
#include <cmath>

struct vector3D
{
    float x, y, z;

    // 基础向量运算重载
    vector3D operator+(const vector3D& v) const { return { x + v.x, y + v.y, z + v.z }; }
    vector3D operator-(const vector3D& v) const { return { x - v.x, y - v.y, z - v.z }; }
    vector3D operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
    bool operator<(const vector3D& other) const
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }

    // 点乘、叉乘、归一化
    float dot(const vector3D& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    vector3D cross(const vector3D& v) const {
        return {
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        };
    }

    vector3D normalize() const {
        float len = std::sqrt(x * x + y * y + z * z);
        if (len == 0) return { 0, 0, 0 };
        return { x / len, y / len, z / len };
    }
};

struct triangle3D
{
    int point[3];
    vector3D NormalVector;
};

struct mesh3D
{
    std::vector<triangle3D> tris;
};

struct matrix
{
    float m[4][4] = { 0 };
};