#pragma once
#include <vector>

struct vector3D
{
	double x, y, z;

	bool operator<(const vector3D& other) const
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

struct triangle3D
{
	vector3D point[3];
};

struct mesh3D
{
	std::vector<triangle3D> tris;
};

struct matrix
{
	double m[4][4] = { 0 };
};