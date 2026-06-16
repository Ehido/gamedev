#include "swrender.h"

#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

uint32_t packRGBA(Vec3 c) {
    auto ch = [](float v) -> uint32_t {
        int i = static_cast<int>(v * 255.f + 0.5f);
        return static_cast<uint32_t>(std::max(0, std::min(255, i)));
    };
    return ch(c.x) | (ch(c.y) << 8) | (ch(c.z) << 16) | (0xFFu << 24);
}

float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

} // namespace

void Framebuffer::clear(Vec3 c) {
    uint32_t v = packRGBA(c);
    std::fill(color.begin(), color.end(), v);
    std::fill(depth.begin(), depth.end(), 1e9f);
}

bool Framebuffer::savePNG(const std::string& path) {
    SDL_Surface* s =
        SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) return false;
    SDL_LockSurface(s);
    for (int y = 0; y < h; ++y) {
        std::memcpy(static_cast<uint8_t*>(s->pixels) + y * s->pitch,
                    color.data() + y * w, w * 4);
    }
    SDL_UnlockSurface(s);
    int rc = IMG_SavePNG(s, path.c_str());
    SDL_FreeSurface(s);
    return rc == 0;
}

void drawTriangle(Framebuffer& fb, const Mat4& viewProj, const Mat4& model,
                  const Vec3& a, const Vec3& b, const Vec3& c, Vec3 baseColor,
                  const Light& light, const Vec3& camPos, const FogParams& fog) {
    // World-space positions (for lighting + fog).
    Vec4 wa4 = model.mul(Vec4(a, 1.f));
    Vec4 wb4 = model.mul(Vec4(b, 1.f));
    Vec4 wc4 = model.mul(Vec4(c, 1.f));
    Vec3 wa{wa4.x, wa4.y, wa4.z}, wb{wb4.x, wb4.y, wb4.z}, wc{wc4.x, wc4.y, wc4.z};

    // Clip-space positions.
    Vec4 ca = viewProj.mul(wa4);
    Vec4 cb = viewProj.mul(wb4);
    Vec4 cc = viewProj.mul(wc4);
    // Skip anything crossing/behind the camera (no near-plane clipping yet).
    if (ca.w < 0.05f || cb.w < 0.05f || cc.w < 0.05f) return;

    // Flat normal, oriented toward the camera so the visible side is lit.
    Vec3 faceCenter = (wa + wb + wc) * (1.f / 3.f);
    Vec3 normal = (wb - wa).cross(wc - wa).normalized();
    if (normal.dot((camPos - faceCenter).normalized()) < 0.f) normal = normal * -1.f;
    float lit = light.ambient + light.diffuse * std::max(0.f, normal.dot(light.dir * -1.f));
    Vec3 shaded = baseColor * lit;

    // Distance fog.
    float dist = (faceCenter - camPos).length();
    float fogT = (dist - fog.start) / std::max(0.001f, fog.end - fog.start);
    fogT = std::max(0.f, std::min(1.f, fogT));
    Vec3 finalCol = shaded * (1.f - fogT) + fog.color * fogT;

    // Perspective divide -> NDC -> screen.
    auto toScreen = [&](const Vec4& cp, float& sx, float& sy, float& sz) {
        float nx = cp.x / cp.w, ny = cp.y / cp.w, nz = cp.z / cp.w;
        sx = (nx * 0.5f + 0.5f) * fb.w;
        sy = (1.f - (ny * 0.5f + 0.5f)) * fb.h;
        sz = nz;  // -1 near .. 1 far
    };
    float ax, ay, az, bx, by, bz, cx, cy, cz;
    toScreen(ca, ax, ay, az);
    toScreen(cb, bx, by, bz);
    toScreen(cc, cx, cy, cz);

    float area = edge(ax, ay, bx, by, cx, cy);
    if (std::fabs(area) < 1e-6f) return;
    float inv = 1.f / area;

    int minX = std::max(0, (int)std::floor(std::min({ax, bx, cx})));
    int maxX = std::min(fb.w - 1, (int)std::ceil(std::max({ax, bx, cx})));
    int minY = std::max(0, (int)std::floor(std::min({ay, by, cy})));
    int maxY = std::min(fb.h - 1, (int)std::ceil(std::max({ay, by, cy})));

    uint32_t packed = packRGBA(finalCol);
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = edge(bx, by, cx, cy, px, py) * inv;
            float w1 = edge(cx, cy, ax, ay, px, py) * inv;
            float w2 = edge(ax, ay, bx, by, px, py) * inv;
            if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;  // outside
            float z = w0 * az + w1 * bz + w2 * cz;
            int idx = y * fb.w + x;
            if (z < fb.depth[idx]) {
                fb.depth[idx] = z;
                fb.color[idx] = packed;
            }
        }
    }
}
