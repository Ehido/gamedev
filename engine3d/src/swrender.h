#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "math3d.h"

// Software rasterizer: a colour + depth framebuffer and a triangle drawer with
// perspective projection, a z-buffer, flat Lambert shading, and distance fog.
// No GPU required -- it runs anywhere and writes the result to a PNG.

struct Framebuffer {
    int w, h;
    std::vector<uint32_t> color;  // bytes in memory: R,G,B,A (SDL RGBA32)
    std::vector<float> depth;     // smaller = closer

    Framebuffer(int W, int H) : w(W), h(H), color(W * H), depth(W * H) {}
    void clear(Vec3 c);
    bool savePNG(const std::string& path);
};

struct Light {
    Vec3 dir;        // direction the light travels
    float ambient;
    float diffuse;
};

struct FogParams {
    Vec3 color;
    float start;     // distance where fog begins
    float end;       // distance where fog is full
};

// Draws one triangle (local-space a/b/c) using the given model + view-proj.
void drawTriangle(Framebuffer& fb, const Mat4& viewProj, const Mat4& model,
                  const Vec3& a, const Vec3& b, const Vec3& c, Vec3 baseColor,
                  const Light& light, const Vec3& camPos, const FogParams& fog);
