// engine3d -- software-rendered 3D engine.
// Renders a textured, lit, fogged scene with an orbiting camera. Single frame
// by default, or an animated sequence with --frames N --outdir DIR.

#include "mesh.h"
#include "swrender.h"

#include <SDL.h>
#include <SDL_image.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void renderMesh(Framebuffer& fb, const Mat4& vp, const Mat4& model, const Mesh& mesh,
                       const Texture* tex, Vec3 tint, const Light& light, const Vec3& eye,
                       const FogParams& fog) {
    for (size_t i = 0; i + 2 < mesh.verts.size(); i += 3)
        drawTriangle(fb, vp, model, mesh.verts[i], mesh.verts[i + 1], mesh.verts[i + 2], tex, tint,
                     light, eye, fog);
}

int main(int argc, char** argv) {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    int frames = 0;
    std::string outdir = ".";
    std::string out = "frame.png";
    std::string assets = "assets";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (a == "--outdir" && i + 1 < argc) outdir = argv[++i];
        else if (a == "--assets" && i + 1 < argc) assets = argv[++i];
        else out = a;
    }

    const int W = 960, H = 600;
    Vec3 fogColor{0.12f, 0.13f, 0.17f};
    Light light{Vec3{-0.5f, -1.f, -0.4f}.normalized(), 0.30f, 0.85f};
    FogParams fog{fogColor, 10.f, 34.f};

    // Meshes + textures (built once).
    Mesh ground = Mesh::plane(60.f, 30.f);
    Mesh sphere = Mesh::uvSphere(24, 36);
    Mesh box = Mesh::cube();
    Mesh octa;
    bool haveObj = octa.loadObj(assets + "/octahedron.obj");

    Texture texGround = Texture::checker(256, {0.10f, 0.12f, 0.13f}, {0.16f, 0.18f, 0.18f}, 24);
    Texture texSphere = Texture::checker(256, {0.85f, 0.32f, 0.18f}, {0.95f, 0.62f, 0.22f}, 14);
    Texture texBox = Texture::checker(128, {0.28f, 0.45f, 0.65f}, {0.18f, 0.28f, 0.45f}, 4);
    Texture texOcta = Texture::checker(128, {0.45f, 0.85f, 0.5f}, {0.2f, 0.5f, 0.25f}, 3);

    Mat4 proj = perspective(60.f * 3.14159265f / 180.f, float(W) / float(H), 0.1f, 200.f);
    Vec3 center{0.f, 1.5f, 0.f};
    const float radius = 9.5f, camH = 5.f;

    int total = frames > 0 ? frames : 1;
    for (int f = 0; f < total; ++f) {
        float a = (frames > 0) ? (float(f) / frames) * 2.f * 3.14159265f : 0.7f;
        Vec3 eye{center.x + std::sin(a) * radius, camH, center.z + std::cos(a) * radius};
        Mat4 vp = proj * lookAt(eye, center, Vec3{0.f, 1.f, 0.f});

        Framebuffer fb(W, H);
        fb.clear(fogColor);

        renderMesh(fb, vp, translate({0.f, 0.f, 0.f}), ground, &texGround, {1, 1, 1}, light, eye, fog);
        renderMesh(fb, vp, translate({0.f, 1.3f, 0.f}) * scale({2.6f, 2.6f, 2.6f}), sphere,
                   &texSphere, {1, 1, 1}, light, eye, fog);
        renderMesh(fb, vp, translate({-3.5f, 1.f, 2.5f}) * rotateY(0.4f) * scale({2.f, 2.f, 2.f}),
                   box, &texBox, {1, 1, 1}, light, eye, fog);
        renderMesh(fb, vp, translate({3.5f, 1.5f, -1.f}) * rotateY(-0.3f) * scale({2.f, 3.f, 2.f}),
                   box, &texBox, {1, 1, 1}, light, eye, fog);
        if (haveObj)
            renderMesh(fb, vp, translate({0.f, 3.6f, -3.f}) * rotateY(a * 2.f) * scale({1.6f, 1.6f, 1.6f}),
                       octa, &texOcta, {1, 1, 1}, light, eye, fog);

        char path[512];
        if (frames > 0)
            std::snprintf(path, sizeof(path), "%s/frame_%03d.png", outdir.c_str(), f);
        else
            std::snprintf(path, sizeof(path), "%s", out.c_str());
        if (!fb.savePNG(path)) {
            std::fprintf(stderr, "savePNG failed: %s\n", path);
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
    }

    IMG_Quit();
    SDL_Quit();
    return 0;
}
