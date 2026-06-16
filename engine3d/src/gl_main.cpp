// engine3d GPU backend -- the same kind of scene rendered with REAL OpenGL
// (GLES 3.0) instead of our CPU rasterizer. Runs headless via EGL's surfaceless
// platform, rendering to an offscreen framebuffer, then reads the pixels back
// to a PNG. On a real machine this is the path to real-time, interactive speed.

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <SDL.h>
#include <SDL_image.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "math3d.h"
#include "mesh.h"

static const char* VS = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vWorld;
out vec2 vUV;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorld = wp.xyz;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* FS = R"(#version 300 es
precision highp float;
in vec3 vWorld;
in vec2 vUV;
uniform vec3 uLightDir;
uniform vec3 uCam;
uniform vec3 uTint;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
out vec4 frag;
void main() {
    float c = mod(floor(vUV.x * 8.0) + floor(vUV.y * 8.0), 2.0);
    vec3 base = mix(uTint * 0.55, uTint, c);
    // Per-fragment flat normal from screen-space derivatives of world pos.
    vec3 N = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    if (dot(N, normalize(uCam - vWorld)) < 0.0) N = -N;
    float lit = 0.3 + 0.85 * max(0.0, dot(N, -normalize(uLightDir)));
    vec3 col = base * lit;
    float d = length(vWorld - uCam);
    float fogT = clamp((d - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    col = mix(col, uFogColor, fogT);
    frag = vec4(col, 1.0);
}
)";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "shader compile error: %s\n", log);
    }
    return s;
}

struct GpuMesh {
    GLuint vao = 0, vbo = 0;
    int count = 0;
};

static GpuMesh upload(const Mesh& m) {
    GpuMesh o;
    o.count = static_cast<int>(m.verts.size());
    glGenVertexArrays(1, &o.vao);
    glBindVertexArray(o.vao);
    glGenBuffers(1, &o.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, o.vbo);
    glBufferData(GL_ARRAY_BUFFER, m.verts.size() * sizeof(Vertex), m.verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 3));
    glBindVertexArray(0);
    return o;
}

int main(int argc, char** argv) {
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);  // no GPU here -> use Mesa llvmpipe
    setenv("EGL_PLATFORM", "surfaceless", 1);
    setenv("GALLIUM_DRIVER", "llvmpipe", 1);

    std::string assets = (argc > 1) ? argv[1] : "assets";
    std::string out = (argc > 2) ? argv[2] : "gl_frame.png";

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
        std::fprintf(stderr, "EGL init failed\n");
        return 1;
    }
    std::printf("EGL %d.%d\n", major, minor);
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                        EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n < 1) {
        std::fprintf(stderr, "eglChooseConfig failed\n");
        return 1;
    }
    EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        std::fprintf(stderr, "EGL context/makeCurrent failed\n");
        return 1;
    }
    std::printf("GL_VERSION:  %s\n", glGetString(GL_VERSION));
    std::printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));

    const int W = 960, H = 600;
    GLuint colorTex;
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLuint depthRb;
    glGenRenderbuffers(1, &depthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "FBO incomplete\n");
        return 1;
    }

    GLuint vs = compile(GL_VERTEX_SHADER, VS), fs = compile(GL_FRAGMENT_SHADER, FS);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "link error: %s\n", log);
        return 1;
    }
    glUseProgram(prog);

    GLint uMVP = glGetUniformLocation(prog, "uMVP");
    GLint uModel = glGetUniformLocation(prog, "uModel");
    GLint uLightDir = glGetUniformLocation(prog, "uLightDir");
    GLint uCam = glGetUniformLocation(prog, "uCam");
    GLint uTint = glGetUniformLocation(prog, "uTint");
    GLint uFogColor = glGetUniformLocation(prog, "uFogColor");
    GLint uFogStart = glGetUniformLocation(prog, "uFogStart");
    GLint uFogEnd = glGetUniformLocation(prog, "uFogEnd");

    Mesh groundM = Mesh::plane(60.f, 30.f);
    Mesh sphereM = Mesh::uvSphere(24, 36);
    Mesh boxM = Mesh::cube();
    Mesh octaM;
    bool haveObj = octaM.loadObj(assets + "/octahedron.obj");
    GpuMesh ground = upload(groundM), sphere = upload(sphereM), box = upload(boxM), octa{};
    if (haveObj) octa = upload(octaM);

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, W, H);
    Vec3 fogColor{0.12f, 0.13f, 0.17f};
    glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Mat4 proj = perspective(60.f * 3.14159265f / 180.f, float(W) / float(H), 0.1f, 200.f);
    Vec3 center{0.f, 1.5f, 0.f};
    float a = 0.7f;
    Vec3 eye{center.x + std::sin(a) * 9.5f, 5.f, center.z + std::cos(a) * 9.5f};
    Mat4 view = lookAt(eye, center, Vec3{0.f, 1.f, 0.f});

    Vec3 ld = Vec3{-0.5f, -1.f, -0.4f}.normalized();
    glUniform3f(uLightDir, ld.x, ld.y, ld.z);
    glUniform3f(uCam, eye.x, eye.y, eye.z);
    glUniform3f(uFogColor, fogColor.x, fogColor.y, fogColor.z);
    glUniform1f(uFogStart, 10.f);
    glUniform1f(uFogEnd, 34.f);

    auto draw = [&](const GpuMesh& o, const Mat4& model, Vec3 tint) {
        Mat4 mvp = proj * view * model;
        glUniformMatrix4fv(uMVP, 1, GL_TRUE, &mvp.m[0][0]);    // GL_TRUE: row-major
        glUniformMatrix4fv(uModel, 1, GL_TRUE, &model.m[0][0]);
        glUniform3f(uTint, tint.x, tint.y, tint.z);
        glBindVertexArray(o.vao);
        glDrawArrays(GL_TRIANGLES, 0, o.count);
    };

    draw(ground, translate({0, 0, 0}), {0.5f, 0.55f, 0.55f});
    draw(sphere, translate({0, 1.3f, 0}) * scale({2.6f, 2.6f, 2.6f}), {0.95f, 0.5f, 0.2f});
    draw(box, translate({-3.5f, 1, 2.5f}) * rotateY(0.4f) * scale({2, 2, 2}), {0.3f, 0.5f, 0.7f});
    draw(box, translate({3.5f, 1.5f, -1}) * rotateY(-0.3f) * scale({2, 3, 2}), {0.3f, 0.5f, 0.7f});
    if (haveObj)
        draw(octa, translate({0, 3.6f, -3}) * rotateY(1.2f) * scale({1.6f, 1.6f, 1.6f}),
             {0.4f, 0.85f, 0.5f});

    glFinish();
    std::vector<unsigned char> pix(static_cast<size_t>(W) * H * 4);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pix.data());

    SDL_Init(0);
    IMG_Init(IMG_INIT_PNG);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_LockSurface(s);
    for (int y = 0; y < H; ++y)  // GL origin is bottom-left -> flip
        std::memcpy(static_cast<unsigned char*>(s->pixels) + y * s->pitch,
                    pix.data() + static_cast<size_t>(H - 1 - y) * W * 4, W * 4);
    SDL_UnlockSurface(s);
    IMG_SavePNG(s, out.c_str());
    SDL_FreeSurface(s);
    std::printf("saved %s\n", out.c_str());

    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return 0;
}
