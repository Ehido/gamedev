#pragma once
#include <cmath>

namespace eng {

// Minimal 2D vector. Kept as a plain value type so it stays cheap to copy
// and lays out contiguously in arrays (cache-friendly for many entities).
struct Vec2 {
    float x = 0.f;
    float y = 0.f;

    Vec2() = default;
    Vec2(float X, float Y) : x(X), y(Y) {}

    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }

    float length() const { return std::sqrt(x * x + y * y); }

    Vec2 normalized() const {
        float l = length();
        return l > 1e-6f ? Vec2{x / l, y / l} : Vec2{0.f, 0.f};
    }
};

} // namespace eng
