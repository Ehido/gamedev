// engine3d -- a from-scratch software-rendered 3D engine (Milestone 0).
// Builds a small scene of lit, fogged cubes and rasterises it to a PNG with our
// own renderer -- no GPU, no Godot. This is the foundation; a GPU backend for
// real-time speed comes later.

#include "swrender.h"

#include <SDL.h>
#include <SDL_image.h>
#include <cstdlib>

namespace {

// Unit cube (-0.5..0.5), 12 triangles. Winding doesn't matter: the shader
// orients each face's normal toward the camera.
const Vec3 CUBE[36] = {
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    {0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f},
    {0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
    {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f},
    {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f},
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f},
};

void renderBox(Framebuffer& fb, const Mat4& vp, Vec3 pos, Vec3 size, float ry,
               Vec3 color, const Light& light, const Vec3& eye, const FogParams& fog) {
    Mat4 model = translate(pos) * rotateY(ry) * scale(size);
    for (int i = 0; i < 36; i += 3)
        drawTriangle(fb, vp, model, CUBE[i], CUBE[i + 1], CUBE[i + 2], color, light, eye, fog);
}

} // namespace

int main(int argc, char** argv) {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    const int W = 960, H = 600;
    Framebuffer fb(W, H);
    Vec3 fogColor{0.12f, 0.13f, 0.17f};
    fb.clear(fogColor);

    Mat4 proj = perspective(60.f * 3.14159265f / 180.f, float(W) / float(H), 0.1f, 200.f);
    Vec3 eye{0.f, 5.f, 14.f};
    Mat4 view = lookAt(eye, Vec3{0.f, 1.5f, -2.f}, Vec3{0.f, 1.f, 0.f});
    Mat4 vp = proj * view;

    Light light{Vec3{-0.5f, -1.f, -0.4f}.normalized(), 0.28f, 0.85f};
    FogParams fog{fogColor, 8.f, 30.f};

    renderBox(fb, vp, {0.f, -0.5f, 0.f}, {60.f, 1.f, 60.f}, 0.f, {0.14f, 0.16f, 0.15f}, light, eye, fog);
    renderBox(fb, vp, {0.f, 1.f, 8.f}, {2.f, 2.f, 2.f}, 0.3f, {0.55f, 0.45f, 0.35f}, light, eye, fog);
    renderBox(fb, vp, {3.f, 1.5f, 4.f}, {2.f, 3.f, 2.f}, 0.f, {0.40f, 0.45f, 0.55f}, light, eye, fog);
    renderBox(fb, vp, {-3.f, 1.f, 3.f}, {2.f, 2.f, 2.f}, 0.6f, {0.50f, 0.40f, 0.45f}, light, eye, fog);
    renderBox(fb, vp, {1.f, 2.f, 0.f}, {2.f, 4.f, 2.f}, 0.2f, {0.45f, 0.50f, 0.45f}, light, eye, fog);
    renderBox(fb, vp, {-2.f, 1.f, -4.f}, {3.f, 2.f, 3.f}, 0.f, {0.50f, 0.48f, 0.40f}, light, eye, fog);
    renderBox(fb, vp, {4.f, 1.f, -3.f}, {2.f, 2.f, 2.f}, 0.4f, {0.42f, 0.46f, 0.52f}, light, eye, fog);

    const char* out = (argc > 1) ? argv[1] : "frame.png";
    bool ok = fb.savePNG(out);

    IMG_Quit();
    SDL_Quit();
    return ok ? 0 : 1;
}
