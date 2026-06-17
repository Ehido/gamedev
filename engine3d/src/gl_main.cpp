// engine3d GPU backend with RAYMARCHED VOLUMETRIC FOG.
// For each pixel we march from the camera to the surface and accumulate fog
// density along the ray (Beer-Lambert). Density = base * height-falloff *
// animated 3D noise, so the fog has drifting patches, pools on the floor, and
// is naturally thicker across open space (longer ray) than near clutter.
//
// Headless via EGL surfaceless + Mesa. `--frames N --outdir DIR` renders an
// orbit/flythrough so the moving, world-space fog is visible.

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
uniform float uTime;
uniform float uFogDensity;
uniform float uHeightFalloff;
uniform float uNoiseScale;
out vec4 frag;

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float vnoise(vec3 x) {
    vec3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash(i + vec3(0,0,0)), n100 = hash(i + vec3(1,0,0));
    float n010 = hash(i + vec3(0,1,0)), n110 = hash(i + vec3(1,1,0));
    float n001 = hash(i + vec3(0,0,1)), n101 = hash(i + vec3(1,0,1));
    float n011 = hash(i + vec3(0,1,1)), n111 = hash(i + vec3(1,1,1));
    float nx00 = mix(n000, n100, f.x), nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x), nx11 = mix(n011, n111, f.x);
    return mix(mix(nx00, nx10, f.y), mix(nx01, nx11, f.y), f.z);
}
float fbm(vec3 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { v += a * vnoise(p); p = p * 2.02; a *= 0.5; }
    return v;
}

// Windy, strand-like fog: anisotropic stretching elongates features along the
// wind, domain warping swirls them into tendrils, and ridged noise sharpens
// them into wisps. Still floor-hugging via the height falloff.
float density(vec3 p) {
    float ground = exp(-max(p.y, 0.0) * uHeightFalloff);
    vec3 wind = vec3(uTime * 0.20, 0.0, uTime * 0.07);
    // Compress along x so features stretch into long strands along the wind.
    vec3 sp = vec3(p.x * 0.32, p.y * 1.3, p.z * 0.95) * uNoiseScale + wind;
    // Domain warp -> swirling filaments.
    vec2 warp = vec2(fbm(sp + 1.7), fbm(sp + 8.3));
    vec3 q = sp + vec3(warp.x - 0.5, 0.0, warp.y - 0.5) * 2.8;
    // Ridged noise -> thin, sharp strands.
    float n = fbm(q);
    float strand = 1.0 - abs(n * 2.0 - 1.0);
    strand = pow(strand, 3.0);
    return uFogDensity * ground * (0.12 + 4.2 * strand);
}
void main() {
    float c = mod(floor(vUV.x * 8.0) + floor(vUV.y * 8.0), 2.0);
    vec3 base = mix(uTint * 0.55, uTint, c);
    vec3 N = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    if (dot(N, normalize(uCam - vWorld)) < 0.0) N = -N;
    float lit = 0.3 + 0.85 * max(0.0, dot(N, -normalize(uLightDir)));
    vec3 col = base * lit;

    // March camera -> surface, accumulating extinction.
    vec3 rd = vWorld - uCam;
    float dist = length(rd);
    rd /= max(dist, 1e-4);
    const int STEPS = 20;
    float stepLen = dist / float(STEPS);
    float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    float trans = 1.0;
    for (int i = 0; i < STEPS; i++) {
        vec3 p = uCam + rd * (stepLen * (float(i) + jitter));
        trans *= exp(-density(p) * stepLen);
    }
    col = mix(uFogColor, col, trans);   // trans=1 clear, 0 full fog
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
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
    setenv("EGL_PLATFORM", "surfaceless", 1);
    setenv("GALLIUM_DRIVER", "llvmpipe", 1);

    std::string assets = "assets";
    std::string out = "gl_frame.png";
    int frames = 0;
    std::string outdir = ".";
    int W = 960, H = 600;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (a == "--outdir" && i + 1 < argc) outdir = argv[++i];
        else if (a == "--assets" && i + 1 < argc) assets = argv[++i];
        else if (a == "--size" && i + 2 < argc) { W = std::atoi(argv[++i]); H = std::atoi(argv[++i]); }
        else out = a;
    }

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor;
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
        std::fprintf(stderr, "EGL init failed\n");
        return 1;
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                        EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLConfig cfg;
    EGLint n = 0;
    eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n);
    EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        std::fprintf(stderr, "EGL context failed\n");
        return 1;
    }
    std::printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));

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
    GLint uTime = glGetUniformLocation(prog, "uTime");
    GLint uFogDensity = glGetUniformLocation(prog, "uFogDensity");
    GLint uHeightFalloff = glGetUniformLocation(prog, "uHeightFalloff");
    GLint uNoiseScale = glGetUniformLocation(prog, "uNoiseScale");

    Mesh groundM = Mesh::plane(60.f, 30.f);
    Mesh sphereM = Mesh::uvSphere(24, 36);
    Mesh boxM = Mesh::cube();
    Mesh octaM;
    bool haveObj = octaM.loadObj(assets + "/octahedron.obj");
    GpuMesh ground = upload(groundM), sphere = upload(sphereM), box = upload(boxM), octa{};
    if (haveObj) octa = upload(octaM);

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, W, H);
    Vec3 fogColor{0.55f, 0.58f, 0.66f};

    Vec3 ld = Vec3{-0.5f, -1.f, -0.4f}.normalized();
    Mat4 proj = perspective(60.f * 3.14159265f / 180.f, float(W) / float(H), 0.1f, 200.f);
    Vec3 center{0.f, 1.0f, 0.f};

    int total = frames > 0 ? frames : 1;
    for (int f = 0; f < total; ++f) {
        float t = float(f) * 0.6f;
        float a = (frames > 0) ? (float(f) / frames) * 2.f * 3.14159265f : 0.7f;
        Vec3 eye{center.x + std::sin(a) * 9.5f, 3.2f, center.z + std::cos(a) * 9.5f};
        Mat4 view = lookAt(eye, center, Vec3{0.f, 1.f, 0.f});

        glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUniform3f(uLightDir, ld.x, ld.y, ld.z);
        glUniform3f(uCam, eye.x, eye.y, eye.z);
        glUniform3f(uFogColor, fogColor.x, fogColor.y, fogColor.z);
        glUniform1f(uTime, t);
        glUniform1f(uFogDensity, 0.26f);
        glUniform1f(uHeightFalloff, 0.30f);
        glUniform1f(uNoiseScale, 0.14f);   // lower = bigger, bolder strands

        auto draw = [&](const GpuMesh& o, const Mat4& model, Vec3 tint) {
            Mat4 mvp = proj * view * model;
            glUniformMatrix4fv(uMVP, 1, GL_TRUE, &mvp.m[0][0]);
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
        for (int y = 0; y < H; ++y)
            std::memcpy(static_cast<unsigned char*>(s->pixels) + y * s->pitch,
                        pix.data() + static_cast<size_t>(H - 1 - y) * W * 4, W * 4);
        SDL_UnlockSurface(s);
        char path[512];
        if (frames > 0) std::snprintf(path, sizeof(path), "%s/fog_%03d.png", outdir.c_str(), f);
        else std::snprintf(path, sizeof(path), "%s", out.c_str());
        IMG_SavePNG(s, path);
        SDL_FreeSurface(s);
    }
    std::printf("done\n");
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return 0;
}
