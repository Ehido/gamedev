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

// Cheap 3D hash -> gradient vector in [-1,1]^3 (no sin).
vec3 hash3(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return -1.0 + 2.0 * fract((p.xxy + p.yzz) * p.zyx);
}
// Gradient (Perlin-style) noise: smooth, no blocky grid. Range ~[-1,1].
float gnoise(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    float n000 = dot(hash3(i + vec3(0,0,0)), f - vec3(0,0,0));
    float n100 = dot(hash3(i + vec3(1,0,0)), f - vec3(1,0,0));
    float n010 = dot(hash3(i + vec3(0,1,0)), f - vec3(0,1,0));
    float n110 = dot(hash3(i + vec3(1,1,0)), f - vec3(1,1,0));
    float n001 = dot(hash3(i + vec3(0,0,1)), f - vec3(0,0,1));
    float n101 = dot(hash3(i + vec3(1,0,1)), f - vec3(1,0,1));
    float n011 = dot(hash3(i + vec3(0,1,1)), f - vec3(0,1,1));
    float n111 = dot(hash3(i + vec3(1,1,1)), f - vec3(1,1,1));
    float nx00 = mix(n000, n100, u.x), nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x), nx11 = mix(n011, n111, u.x);
    return mix(mix(nx00, nx10, u.y), mix(nx01, nx11, u.y), u.z);
}
float fbm(vec3 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { v += a * gnoise(p); p = p * 2.03; a *= 0.5; }
    return v;
}

// Streams of fog only: pure clear air with a few thin, sharp, wind-stretched
// strands. No ambient/floor haze -- density is zero except inside the strands.
float density(vec3 p) {
    vec3 wind = vec3(uTime * 0.18, 0.0, uTime * 0.05);
    // Streams stretched along the wind, but not so long they pile up opaque.
    vec3 sp = vec3(p.x * 0.40, p.y * 1.5, p.z * 0.95) * uNoiseScale + wind;
    float w = fbm(sp * 0.7);
    vec3 q = sp + vec3(0.0, 0.0, w * 0.7);
    float s = pow(1.0 - abs(gnoise(q)), 9.0);    // thin, sharp strand
    s = smoothstep(0.32, 0.78, s);               // crisp; fully clear between
    // Foot fog fills up to leg height then dissipates before the eyes; thicker
    // fog (higher density) rises higher up the legs -> exposed as a setting later.
    float legTop = 0.6 + uFogDensity * 0.5;
    float footFog = 1.0 - smoothstep(legTop, legTop + 0.5, p.y);
    float headFog = smoothstep(1.9, 3.4, p.y);            // above the head
    float profile = max(footFog, headFog);
    return uFogDensity * s * profile;             // streams, gated to legs + overhead
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
    const int STEPS = 28;
    float stepLen = dist / float(STEPS);
    float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    float trans = 1.0;
    vec3 fogCol = vec3(0.0);
    for (int i = 0; i < STEPS; i++) {
        vec3 p = uCam + rd * (stepLen * (float(i) + jitter));
        float d = density(p);
        if (d > 0.0001) {
            // Each stream's grey varies dark -> light from a low-freq noise.
            float g = gnoise(p * 0.13 + vec3(uTime * 0.05, 0.0, 0.0));
            vec3 shade = vec3(mix(0.30, 0.92, clamp(g * 0.5 + 0.5, 0.0, 1.0)));
            float a = 1.0 - exp(-d * stepLen);
            fogCol += trans * a * shade;
            trans *= (1.0 - a);
        }
    }
    col = col * trans + fogCol;   // scene behind the fog + accumulated fog colour
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
    float fogDensity = 0.6f, noiseScale = 0.31f;   // locked from the variation grid (#1-#4 band)
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (a == "--outdir" && i + 1 < argc) outdir = argv[++i];
        else if (a == "--assets" && i + 1 < argc) assets = argv[++i];
        else if (a == "--size" && i + 2 < argc) { W = std::atoi(argv[++i]); H = std::atoi(argv[++i]); }
        else if (a == "--density" && i + 1 < argc) fogDensity = std::atof(argv[++i]);
        else if (a == "--scale" && i + 1 < argc) noiseScale = std::atof(argv[++i]);
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
    Vec3 center{0.f, 1.4f, 0.f};   // look roughly at eye level

    int total = frames > 0 ? frames : 1;
    for (int f = 0; f < total; ++f) {
        float t = float(f) * 0.18f;
        float a = (frames > 0) ? (float(f) / frames) * 2.f * 3.14159265f : 0.7f;
        Vec3 eye{center.x + std::sin(a) * 8.5f, 1.7f, center.z + std::cos(a) * 8.5f};
        Mat4 view = lookAt(eye, center, Vec3{0.f, 1.f, 0.f});

        glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUniform3f(uLightDir, ld.x, ld.y, ld.z);
        glUniform3f(uCam, eye.x, eye.y, eye.z);
        glUniform3f(uFogColor, fogColor.x, fogColor.y, fogColor.z);
        glUniform1f(uTime, t);
        glUniform1f(uFogDensity, fogDensity);
        glUniform1f(uHeightFalloff, 0.30f);
        glUniform1f(uNoiseScale, noiseScale);

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
