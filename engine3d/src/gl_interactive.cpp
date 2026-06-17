// engine3d -- interactive first-person walk-through (Windows / desktop OpenGL).
// SDL2 window + OpenGL 3.3 core. WASD move, mouse look, Shift sprint, F toggles
// the flashlight, 1-6 tune the fog live, Esc quits. Walk through the foggy ruin.

#include <SDL.h>
#include <SDL_opengl.h>

#include <cmath>
#include <cstdio>
#include <vector>

#include "math3d.h"
#include "mesh.h"

// --- Load the GL 2.0+ entry points (not exported by opengl32 on Windows) ----
#define GLFUNCS \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
    X(PFNGLUNIFORM3FPROC, glUniform3f) \
    X(PFNGLUNIFORM1FPROC, glUniform1f)

#define X(type, name) static type name = nullptr;
GLFUNCS
#undef X

static bool loadGL() {
    bool ok = true;
#define X(type, name) name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name)); if (!name) ok = false;
    GLFUNCS
#undef X
    return ok;
}

static const char* VS = R"(#version 330 core
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

static const char* FS = R"(#version 330 core
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
uniform float uRegFogDensity;
uniform vec3 uSpotPos;
uniform vec3 uSpotDir;
uniform float uSpotCos;
uniform vec3 uSpotColor;
uniform float uSpotIntensity;
uniform float uWindSpeed;
out vec4 frag;

vec3 hash3(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return -1.0 + 2.0 * fract((p.xxy + p.yzz) * p.zyx);
}
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
float density(vec3 p) {
    float wt = uTime * uWindSpeed;
    // Steady drift plus a slow lateral sway: the wind direction shifts a little
    // over time while keeping its general heading.
    vec3 wind = vec3(0.18, 0.0, 0.05) * wt + vec3(-0.06, 0.0, 0.20) * (sin(wt * 0.5) * 0.6);
    vec3 sp = vec3(p.x * 0.40, p.y * 1.5, p.z * 0.95) * uNoiseScale + wind;
    float w = gnoise(sp * 0.6);                  // cheap single-octave warp
    vec3 q = sp + vec3(0.0, 0.0, w * 0.7);
    float s = pow(1.0 - abs(gnoise(q)), 10.0);   // a touch softer -> less speckle
    s = smoothstep(0.34, 0.86, s);
    float legTop = 0.6 + uFogDensity * 0.5;
    float footFog = 1.0 - smoothstep(legTop, legTop + 0.5, p.y);
    float headFog = smoothstep(1.9, 3.4, p.y);
    return uFogDensity * s * max(footFog, headFog);
}
vec3 spotInScatter(vec3 p) {
    vec3 toP = p - uSpotPos;
    float dP = length(toP);
    vec3 dirP = toP / max(dP, 1e-4);
    float cone = dot(dirP, uSpotDir);
    float coneFall = smoothstep(uSpotCos, uSpotCos + (1.0 - uSpotCos) * 0.5, cone);
    float distFall = 1.0 / (1.0 + 0.018 * dP * dP);
    return uSpotColor * uSpotIntensity * coneFall * distFall;
}
void main() {
    float c = mod(floor(vUV.x * 8.0) + floor(vUV.y * 8.0), 2.0);
    vec3 base = mix(uTint * 0.55, uTint, c);
    vec3 N = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    if (dot(N, normalize(uCam - vWorld)) < 0.0) N = -N;
    float lit = 0.3 + 0.85 * max(0.0, dot(N, -normalize(uLightDir)));
    vec3 col = base * lit;

    vec3 rd = vWorld - uCam;
    float dist = length(rd);
    rd /= max(dist, 1e-4);
    const int STEPS = 24;
    float stepLen = dist / float(STEPS);
    float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    float trans = 1.0;
    vec3 fogCol = vec3(0.0);
    for (int i = 0; i < STEPS; i++) {
        vec3 p = uCam + rd * (stepLen * (float(i) + jitter));
        float streamD = density(p);
        float med = streamD + uRegFogDensity;
        float a = 1.0 - exp(-med * stepLen);
        float g = gnoise(p * 0.13 + vec3(uTime * 0.05 * uWindSpeed, 0.0, 0.0));
        vec3 streamShade = vec3(mix(0.30, 0.92, clamp(g * 0.5 + 0.5, 0.0, 1.0)));
        vec3 ambient = mix(uFogColor, streamShade, clamp(streamD / max(med, 1e-4), 0.0, 1.0));
        vec3 lightCol = ambient + spotInScatter(p);
        fogCol += trans * a * lightCol;
        trans *= (1.0 - a);
        if (trans < 0.03) break;                 // stop once fully fogged (perf)
    }
    col = col * trans + fogCol;
    frag = vec4(clamp(col, 0.0, 1.0), 1.0);   // clamp so single samples can't blow to white
}
)";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, sizeof(log), nullptr, log); SDL_Log("shader: %s", log); }
    return s;
}

struct GpuMesh { GLuint vao = 0, vbo = 0; int count = 0; };

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

struct BoxInst { Vec3 pos, size, tint; float rotY = 0.f; };

static std::vector<BoxInst> buildScene() {
    std::vector<BoxInst> v;
    auto add = [&](Vec3 p, Vec3 s, Vec3 t, float r = 0.f) { v.push_back({p, s, t, r}); };
    Vec3 wall{0.30f, 0.30f, 0.34f}, pillar{0.26f, 0.27f, 0.32f}, crate{0.42f, 0.34f, 0.24f};
    const float zb = -38.f, zf = 30.f, zc = (zb + zf) * 0.5f, zl = zf - zb;
    add({-11.f, 3.f, zc}, {1.f, 6.f, zl}, wall);
    add({11.f, 3.f, zc}, {1.f, 6.f, zl}, wall);
    add({0.f, 3.f, zb}, {22.f, 6.f, 1.f}, wall);
    for (float z = zf - 6.f; z > zb + 4.f; z -= 8.f) {
        add({-6.f, 2.5f, z}, {1.4f, 5.f, 1.4f}, pillar);
        add({6.f, 2.5f, z}, {1.4f, 5.f, 1.4f}, pillar);
    }
    add({-2.f, 1.2f, -8.f}, {8.f, 2.4f, 1.f}, wall, 0.2f);
    unsigned seed = 1234u;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return ((seed >> 16) & 0x7fff) / 32767.f; };
    for (int i = 0; i < 44; ++i) {
        float x = -8.5f + rnd() * 17.f, z = zb + 5.f + rnd() * (zl - 10.f), s = 0.6f + rnd() * 1.7f;
        add({x, s * 0.5f, z}, {s, s, s}, crate, rnd() * 6.28f);
    }
    return v;
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { SDL_Log("SDL init: %s", SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int W = 1280, H = 720;
    SDL_Window* win = SDL_CreateWindow("MURK - walk (engine3d)", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!win) { SDL_Log("window: %s", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { SDL_Log("gl context: %s", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    if (!loadGL()) { SDL_Log("failed to load GL functions"); return 1; }
    SDL_SetRelativeMouseMode(SDL_TRUE);

    GLuint vs = compile(GL_VERTEX_SHADER, VS), fs = compile(GL_FRAGMENT_SHADER, FS);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint linked = 0; glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) { char log[2048]; glGetProgramInfoLog(prog, sizeof(log), nullptr, log); SDL_Log("link: %s", log); return 1; }
    glUseProgram(prog);

    GLint uMVP = glGetUniformLocation(prog, "uMVP"), uModel = glGetUniformLocation(prog, "uModel");
    GLint uLightDir = glGetUniformLocation(prog, "uLightDir"), uCam = glGetUniformLocation(prog, "uCam");
    GLint uTint = glGetUniformLocation(prog, "uTint"), uFogColor = glGetUniformLocation(prog, "uFogColor");
    GLint uTime = glGetUniformLocation(prog, "uTime"), uFogDensity = glGetUniformLocation(prog, "uFogDensity");
    GLint uHeightFalloff = glGetUniformLocation(prog, "uHeightFalloff"), uNoiseScale = glGetUniformLocation(prog, "uNoiseScale");
    GLint uRegFogDensity = glGetUniformLocation(prog, "uRegFogDensity");
    GLint uSpotPos = glGetUniformLocation(prog, "uSpotPos"), uSpotDir = glGetUniformLocation(prog, "uSpotDir");
    GLint uSpotCos = glGetUniformLocation(prog, "uSpotCos"), uSpotColor = glGetUniformLocation(prog, "uSpotColor");
    GLint uSpotIntensity = glGetUniformLocation(prog, "uSpotIntensity");
    GLint uWindSpeed = glGetUniformLocation(prog, "uWindSpeed");

    Mesh groundM = Mesh::plane(120.f, 60.f), boxM = Mesh::cube();
    GpuMesh ground = upload(groundM), box = upload(boxM);
    std::vector<BoxInst> scene = buildScene();

    glEnable(GL_DEPTH_TEST);
    Vec3 fogColor{0.62f, 0.63f, 0.66f};
    Vec3 ld = Vec3{-0.5f, -1.f, -0.4f}.normalized();

    Vec3 pos{0.f, 1.7f, 22.f};
    float yaw = 3.14159265f, pitch = 0.f;   // looking -z
    float fogDensity = 0.6f, noiseScale = 0.31f, regFog = 0.028f, spotI = 0.95f, windSpeed = 2.5f;
    float t = 0.f;

    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = double(SDL_GetPerformanceFrequency());
    bool running = true;
    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = float((now - prev) / freq); prev = now;
        if (dt > 0.1f) dt = 0.1f;
        t += dt;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_MOUSEMOTION) {
                yaw += e.motion.xrel * 0.0025f;
                pitch -= e.motion.yrel * 0.0025f;
                if (pitch > 1.5f) pitch = 1.5f; if (pitch < -1.5f) pitch = -1.5f;
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                W = e.window.data1; H = e.window.data2;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_f: spotI = (spotI > 0.f ? 0.f : 0.95f); break;
                    case SDLK_1: regFog = fmaxf(0.f, regFog - 0.004f); break;
                    case SDLK_2: regFog += 0.004f; break;
                    case SDLK_3: fogDensity = fmaxf(0.f, fogDensity - 0.05f); break;
                    case SDLK_4: fogDensity += 0.05f; break;
                    case SDLK_5: noiseScale = fmaxf(0.05f, noiseScale - 0.02f); break;
                    case SDLK_6: noiseScale += 0.02f; break;
                    case SDLK_7: windSpeed = fmaxf(0.f, windSpeed - 0.5f); break;
                    case SDLK_8: windSpeed += 0.5f; break;
                }
            }
        }

        Vec3 fwd{std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
        Vec3 fwdH = Vec3{fwd.x, 0.f, fwd.z}.normalized();
        Vec3 right = fwdH.cross(Vec3{0, 1, 0}).normalized();
        const Uint8* k = SDL_GetKeyboardState(nullptr);
        float spd = (k[SDL_SCANCODE_LSHIFT] ? 9.f : 4.5f);
        Vec3 move{0, 0, 0};
        if (k[SDL_SCANCODE_W]) move += fwdH;
        if (k[SDL_SCANCODE_S]) move = move - fwdH;
        if (k[SDL_SCANCODE_D]) move += right;
        if (k[SDL_SCANCODE_A]) move = move - right;
        if (move.length() > 0.001f) pos += move.normalized() * (spd * dt);
        pos.y = 1.7f;

        glViewport(0, 0, W, H);
        glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Mat4 proj = perspective(60.f * 3.14159265f / 180.f, float(W) / float(H), 0.1f, 200.f);
        Mat4 view = lookAt(pos, pos + fwd, Vec3{0, 1, 0});

        glUniform3f(uLightDir, ld.x, ld.y, ld.z);
        glUniform3f(uCam, pos.x, pos.y, pos.z);
        glUniform3f(uFogColor, fogColor.x, fogColor.y, fogColor.z);
        glUniform1f(uTime, t);
        glUniform1f(uFogDensity, fogDensity);
        glUniform1f(uHeightFalloff, 0.30f);
        glUniform1f(uNoiseScale, noiseScale);
        glUniform1f(uRegFogDensity, regFog);
        glUniform3f(uSpotPos, pos.x, pos.y, pos.z);
        glUniform3f(uSpotDir, fwd.x, fwd.y, fwd.z);
        glUniform1f(uSpotCos, std::cos(22.f * 3.14159265f / 180.f));
        glUniform3f(uSpotColor, 1.0f, 0.95f, 0.82f);
        glUniform1f(uSpotIntensity, spotI);
        glUniform1f(uWindSpeed, windSpeed);

        auto draw = [&](const GpuMesh& o, const Mat4& model, Vec3 tint) {
            Mat4 mvp = proj * view * model;
            glUniformMatrix4fv(uMVP, 1, GL_TRUE, &mvp.m[0][0]);
            glUniformMatrix4fv(uModel, 1, GL_TRUE, &model.m[0][0]);
            glUniform3f(uTint, tint.x, tint.y, tint.z);
            glBindVertexArray(o.vao);
            glDrawArrays(GL_TRIANGLES, 0, o.count);
        };

        draw(ground, translate({0, 0, 0}), {0.42f, 0.44f, 0.46f});
        for (const BoxInst& b : scene)
            draw(box, translate(b.pos) * rotateY(b.rotY) * scale(b.size), b.tint);

        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
