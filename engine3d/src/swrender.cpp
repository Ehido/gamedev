#include "swrender.h"

#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstring>

static uint32_t packRGBA(Vec3 c) {
    auto ch = [](float v) -> uint32_t {
        int i = static_cast<int>(v * 255.f + 0.5f);
        return static_cast<uint32_t>(std::max(0, std::min(255, i)));
    };
    return ch(c.x) | (ch(c.y) << 8) | (ch(c.z) << 16) | (0xFFu << 24);
}

void Framebuffer::clear(Vec3 c) {
    std::fill(color.begin(), color.end(), packRGBA(c));
    std::fill(depth.begin(), depth.end(), 1e9f);
}

bool Framebuffer::savePNG(const std::string& path) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) return false;
    SDL_LockSurface(s);
    for (int y = 0; y < h; ++y)
        std::memcpy(static_cast<uint8_t*>(s->pixels) + y * s->pitch, color.data() + y * w, w * 4);
    SDL_UnlockSurface(s);
    int rc = IMG_SavePNG(s, path.c_str());
    SDL_FreeSurface(s);
    return rc == 0;
}

Vec3 Texture::sample(float u, float v) const {
    if (!valid()) return {1.f, 1.f, 1.f};
    u = u - std::floor(u);
    v = v - std::floor(v);
    int x = std::min(w - 1, static_cast<int>(u * w));
    int y = std::min(h - 1, static_cast<int>(v * h));
    uint32_t p = px[static_cast<size_t>(y) * w + x];
    return {(p & 0xFF) / 255.f, ((p >> 8) & 0xFF) / 255.f, ((p >> 16) & 0xFF) / 255.f};
}

Texture Texture::checker(int size, Vec3 a, Vec3 b, int cells) {
    Texture t;
    t.w = size;
    t.h = size;
    t.px.resize(static_cast<size_t>(size) * size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            int cx = x * cells / size, cy = y * cells / size;
            t.px[static_cast<size_t>(y) * size + x] = packRGBA(((cx + cy) & 1) ? a : b);
        }
    return t;
}

bool Texture::load(const std::string& path) {
    SDL_Surface* img = IMG_Load(path.c_str());
    if (!img) return false;
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(img);
    if (!conv) return false;
    w = conv->w;
    h = conv->h;
    px.resize(static_cast<size_t>(w) * h);
    SDL_LockSurface(conv);
    for (int y = 0; y < h; ++y)
        std::memcpy(px.data() + static_cast<size_t>(y) * w,
                    static_cast<uint8_t*>(conv->pixels) + y * conv->pitch, w * 4);
    SDL_UnlockSurface(conv);
    SDL_FreeSurface(conv);
    return true;
}

namespace {

// Clip-space vertex carrying the texture coords through clipping.
struct CV {
    Vec4 clip;
    float u, v;
};

CV lerpCV(const CV& a, const CV& b, float t) {
    CV r;
    r.clip = Vec4(a.clip.x + (b.clip.x - a.clip.x) * t, a.clip.y + (b.clip.y - a.clip.y) * t,
                  a.clip.z + (b.clip.z - a.clip.z) * t, a.clip.w + (b.clip.w - a.clip.w) * t);
    r.u = a.u + (b.u - a.u) * t;
    r.v = a.v + (b.v - a.v) * t;
    return r;
}

float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

} // namespace

void drawTriangle(Framebuffer& fb, const Mat4& viewProj, const Mat4& model,
                  const Vertex& A, const Vertex& B, const Vertex& C,
                  const Texture* tex, Vec3 tint, const Light& light,
                  const Vec3& camPos, const FogParams& fog) {
    // World-space (lighting + fog).
    Vec4 wa4 = model.mul(Vec4(A.pos, 1.f));
    Vec4 wb4 = model.mul(Vec4(B.pos, 1.f));
    Vec4 wc4 = model.mul(Vec4(C.pos, 1.f));
    Vec3 wa{wa4.x, wa4.y, wa4.z}, wb{wb4.x, wb4.y, wb4.z}, wc{wc4.x, wc4.y, wc4.z};
    Vec3 faceC = (wa + wb + wc) * (1.f / 3.f);
    Vec3 n = (wb - wa).cross(wc - wa).normalized();
    if (n.dot((camPos - faceC).normalized()) < 0.f) n = n * -1.f;
    float lit = light.ambient + light.diffuse * std::max(0.f, n.dot(light.dir * -1.f));
    float fogT = (faceC - camPos).length();
    fogT = (fogT - fog.start) / std::max(0.001f, fog.end - fog.start);
    fogT = std::max(0.f, std::min(1.f, fogT));

    // Near-plane clip (z + w >= 0), carrying uv.
    CV poly[3] = {{viewProj.mul(wa4), A.u, A.v},
                  {viewProj.mul(wb4), B.u, B.v},
                  {viewProj.mul(wc4), C.u, C.v}};
    CV out[6];
    int oc = 0;
    for (int i = 0; i < 3; ++i) {
        CV cur = poly[i], nxt = poly[(i + 1) % 3];
        float dc = cur.clip.z + cur.clip.w, dn = nxt.clip.z + nxt.clip.w;
        if (dc >= 0.f) out[oc++] = cur;
        if ((dc >= 0.f) != (dn >= 0.f)) out[oc++] = lerpCV(cur, nxt, dc / (dc - dn));
    }
    if (oc < 3) return;

    struct SV {
        float x, y, z, invw, uw, vw;
    };
    SV sv[6];
    for (int i = 0; i < oc; ++i) {
        float inv = 1.f / out[i].clip.w;
        sv[i].x = (out[i].clip.x * inv * 0.5f + 0.5f) * fb.w;
        sv[i].y = (1.f - (out[i].clip.y * inv * 0.5f + 0.5f)) * fb.h;
        sv[i].z = out[i].clip.z * inv;
        sv[i].invw = inv;
        sv[i].uw = out[i].u * inv;
        sv[i].vw = out[i].v * inv;
    }

    // Fan-triangulate the clipped polygon and rasterise.
    for (int i = 1; i + 1 < oc; ++i) {
        SV a = sv[0], b = sv[i], c = sv[i + 1];
        float area = edge(a.x, a.y, b.x, b.y, c.x, c.y);
        if (std::fabs(area) < 1e-6f) continue;
        float invArea = 1.f / area;
        int minX = std::max(0, (int)std::floor(std::min({a.x, b.x, c.x})));
        int maxX = std::min(fb.w - 1, (int)std::ceil(std::max({a.x, b.x, c.x})));
        int minY = std::max(0, (int)std::floor(std::min({a.y, b.y, c.y})));
        int maxY = std::min(fb.h - 1, (int)std::ceil(std::max({a.y, b.y, c.y})));
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float px = x + 0.5f, py = y + 0.5f;
                float w0 = edge(b.x, b.y, c.x, c.y, px, py) * invArea;
                float w1 = edge(c.x, c.y, a.x, a.y, px, py) * invArea;
                float w2 = edge(a.x, a.y, b.x, b.y, px, py) * invArea;
                if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;
                float z = w0 * a.z + w1 * b.z + w2 * c.z;
                int idx = y * fb.w + x;
                if (z >= fb.depth[idx]) continue;
                float invw = w0 * a.invw + w1 * b.invw + w2 * c.invw;
                float u = (w0 * a.uw + w1 * b.uw + w2 * c.uw) / invw;
                float v = (w0 * a.vw + w1 * b.vw + w2 * c.vw) / invw;
                Vec3 base = tex ? tex->sample(u, v) : Vec3{1.f, 1.f, 1.f};
                Vec3 col{base.x * tint.x * lit, base.y * tint.y * lit, base.z * tint.z * lit};
                col = col * (1.f - fogT) + fog.color * fogT;
                fb.depth[idx] = z;
                fb.color[idx] = packRGBA(col);
            }
        }
    }
}
