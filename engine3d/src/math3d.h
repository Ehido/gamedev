#pragma once
#include <cmath>

// Minimal 3D math for the engine: vectors and a 4x4 matrix with the usual
// transform builders (translate / scale / rotate / perspective / lookAt).
// Matrices are row-major; a point is transformed as v' = M * v.

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() {}
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        float l = length();
        return l > 1e-6f ? Vec3{x / l, y / l, z / l} : Vec3{};
    }
};

struct Vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
    Vec4() {}
    Vec4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
    Vec4(const Vec3& v, float W) : x(v.x), y(v.y), z(v.z), w(W) {}
};

struct Mat4 {
    float m[4][4];

    static Mat4 identity() {
        Mat4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) r.m[i][j] = (i == j) ? 1.f : 0.f;
        return r;
    }

    Vec4 mul(const Vec4& v) const {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
            m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w};
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                float s = 0.f;
                for (int k = 0; k < 4; ++k) s += m[i][k] * o.m[k][j];
                r.m[i][j] = s;
            }
        return r;
    }
};

inline Mat4 translate(const Vec3& t) {
    Mat4 r = Mat4::identity();
    r.m[0][3] = t.x; r.m[1][3] = t.y; r.m[2][3] = t.z;
    return r;
}

inline Mat4 scale(const Vec3& s) {
    Mat4 r = Mat4::identity();
    r.m[0][0] = s.x; r.m[1][1] = s.y; r.m[2][2] = s.z;
    return r;
}

inline Mat4 rotateY(float a) {
    Mat4 r = Mat4::identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0][0] = c; r.m[0][2] = s; r.m[2][0] = -s; r.m[2][2] = c;
    return r;
}

inline Mat4 perspective(float fovy, float aspect, float zn, float zf) {
    Mat4 r{};
    float f = 1.f / std::tan(fovy * 0.5f);
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = (zf + zn) / (zn - zf);
    r.m[2][3] = (2.f * zf * zn) / (zn - zf);
    r.m[3][2] = -1.f;
    return r;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = (center - eye).normalized();
    Vec3 s = f.cross(up).normalized();
    Vec3 u = s.cross(f);
    Mat4 r = Mat4::identity();
    r.m[0][0] = s.x; r.m[0][1] = s.y; r.m[0][2] = s.z;
    r.m[1][0] = u.x; r.m[1][1] = u.y; r.m[1][2] = u.z;
    r.m[2][0] = -f.x; r.m[2][1] = -f.y; r.m[2][2] = -f.z;
    r.m[0][3] = -s.dot(eye);
    r.m[1][3] = -u.dot(eye);
    r.m[2][3] = f.dot(eye);
    return r;
}
