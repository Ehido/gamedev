#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "math3d.h"

// Software rasterizer with near-plane clipping, perspective-correct attribute
// interpolation, textures, flat Lambert shading and distance fog. CPU-only,
// writes to PNG.

struct Framebuffer {
    int w, h;
    std::vector<uint32_t> color;  // R,G,B,A bytes (SDL RGBA32)
    std::vector<float> depth;     // smaller = closer
    Framebuffer(int W, int H) : w(W), h(H), color(W * H), depth(W * H) {}
    void clear(Vec3 c);
    bool savePNG(const std::string& path);
};

struct Texture {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    bool valid() const { return w > 0 && h > 0 && !px.empty(); }
    Vec3 sample(float u, float v) const;                 // wrap + nearest
    static Texture checker(int size, Vec3 a, Vec3 b, int cells);
    bool load(const std::string& path);
};

struct Vertex {
    Vec3 pos;
    float u = 0.f, v = 0.f;
};

struct Light {
    Vec3 dir;
    float ambient;
    float diffuse;
};

struct FogParams {
    Vec3 color;
    float start;
    float end;
};

void drawTriangle(Framebuffer& fb, const Mat4& viewProj, const Mat4& model,
                  const Vertex& a, const Vertex& b, const Vertex& c,
                  const Texture* tex, Vec3 tint, const Light& light,
                  const Vec3& camPos, const FogParams& fog);
