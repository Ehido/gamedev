// engine3d -- interactive first-person walk-through (Windows / desktop OpenGL).
// Deferred fog renderer: geometry is drawn crisp at full resolution, the heavy
// volumetric fog is raymarched at low resolution, and the two are composited.
// This keeps edges sharp while making the fog cheap.
//
// WASD move, Space jump, Ctrl crouch, Shift sprint, mouse look, F flashlight,
// 1-8 tune the fog, 9/0 sharper/faster fog, Esc quits.

#include <SDL.h>
#include <SDL_opengl.h>

#include <cmath>
#include <cstdio>
#include <vector>

#include "math3d.h"
#include "mesh.h"

// --- Load GL 2.0+ / FBO entry points (not exported by opengl32 on Windows) ---
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
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM3FVPROC, glUniform3fv) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)

#define X(type, name) static type name = nullptr;
GLFUNCS
#undef X
// glActiveTexture is already declared by gl.h (GL 1.3), so load it separately.
static PFNGLACTIVETEXTUREPROC pglActiveTexture = nullptr;

static bool loadGL() {
    bool ok = true;
#define X(type, name) name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name)); if (!name) ok = false;
    GLFUNCS
#undef X
    pglActiveTexture = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(SDL_GL_GetProcAddress("glActiveTexture"));
    if (!pglActiveTexture) ok = false;
    return ok;
}

// Geometry vertex shader.
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

// Geometry fragment shader: lit checker + coloured point lights. NO fog.
static const char* SCENE_FS = R"(#version 330 core
in vec3 vWorld;
in vec2 vUV;
uniform vec3 uLightDir;
uniform vec3 uCam;
uniform vec3 uTint;
uniform int uNumLights;
uniform vec3 uLightPos[8];
uniform vec3 uLightCol[8];
out vec4 frag;
void main() {
    float c = mod(floor(vUV.x * 8.0) + floor(vUV.y * 8.0), 2.0);
    vec3 base = mix(uTint * 0.78, uTint, c);   // softer tiling, less programmer-checker
    vec3 N = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    if (dot(N, normalize(uCam - vWorld)) < 0.0) N = -N;
    vec3 col = base * (0.3 + 0.85 * max(0.0, dot(N, -normalize(uLightDir))));
    for (int i = 0; i < uNumLights; i++) {
        vec3 L = uLightPos[i] - vWorld;
        float d = length(L);
        float att = 1.0 / (1.0 + 0.15 * d * d);
        col += base * uLightCol[i] * (max(0.0, dot(N, L / max(d, 1e-4))) * att);
    }
    frag = vec4(col, 1.0);
}
)";

// Fullscreen-triangle vertex shader (no vertex buffer needed).
static const char* FULL_VS = R"(#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    vUv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)";

// Fog fragment shader: reconstruct each pixel's surface from the depth texture,
// raymarch camera->surface accumulating fog. Output rgb=inscatter, a=transmittance.
static const char* FOG_FS = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uDepth;
uniform vec3 uCam, uFwd, uRight, uUp;
uniform float uTanHalf, uAspect, uNear, uFar;
uniform vec3 uFogColor;
uniform float uTime, uFogDensity, uHeightFalloff, uNoiseScale, uRegFogDensity, uWindSpeed;
uniform vec3 uSpotPos, uSpotDir, uSpotColor;
uniform float uSpotCos, uSpotIntensity;
uniform int uNumLights;
uniform vec3 uLightPos[8];
uniform vec3 uLightCol[8];
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
    vec3 wind = vec3(0.18, 0.0, 0.05) * wt + vec3(-0.06, 0.0, 0.20) * (sin(wt * 0.5) * 0.6);
    vec3 sp = vec3(p.x * 0.40, p.y * 1.5, p.z * 0.95) * uNoiseScale + wind;
    float w = gnoise(sp * 0.6);
    vec3 q = sp + vec3(0.0, 0.0, w * 0.7);
    float s = pow(1.0 - abs(gnoise(q)), 10.0);
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
    float coneFall = smoothstep(uSpotCos, uSpotCos + (1.0 - uSpotCos) * 0.5, dot(dirP, uSpotDir));
    return uSpotColor * uSpotIntensity * coneFall * (1.0 / (1.0 + 0.018 * dP * dP));
}
void main() {
    vec2 ndc = vUv * 2.0 - 1.0;
    vec3 rd = normalize(uFwd + uRight * (ndc.x * uTanHalf * uAspect) + uUp * (ndc.y * uTanHalf));
    float depth = texture(uDepth, vUv).r;
    float dist;
    if (depth >= 0.9999) {
        dist = uFar;                                  // no geometry -> march to far
    } else {
        float zndc = depth * 2.0 - 1.0;
        float zeye = (2.0 * uNear * uFar) / (uNear + uFar - zndc * (uFar - uNear));
        dist = zeye / max(dot(rd, uFwd), 1e-3);
    }
    const int STEPS = 14;
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
        vec3 lightCol = ambient + spotInScatter(p) * 0.3;   // subtler beam (no whiteout)
        for (int li = 0; li < uNumLights; li++) {
            vec3 L = uLightPos[li] - p;
            lightCol += uLightCol[li] * (1.0 / (1.0 + 0.25 * dot(L, L)));
        }
        fogCol += trans * a * lightCol;
        trans *= (1.0 - a);
        if (trans < 0.03) break;
    }
    frag = vec4(min(fogCol, vec3(1.0)), trans);   // clamp so the beam can't blow to white
}
)";

// Composite: full-res scene * fog.transmittance + fog.inscatter.
static const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uScene;
uniform sampler2D uFog;
out vec4 frag;
void main() {
    vec3 scene = texture(uScene, vUv).rgb;
    vec4 fog = texture(uFog, vUv);
    frag = vec4(clamp(scene * fog.a + fog.rgb, 0.0, 1.0), 1.0);
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
static GLuint link(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(p, sizeof(log), nullptr, log); SDL_Log("link: %s", log); }
    return p;
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
    add({0.f, 3.f, zb}, {22.f, 6.f, 1.f}, wall);     // back wall
    add({0.f, 3.f, zf}, {22.f, 6.f, 1.f}, wall);     // front wall (enclose)
    add({0.f, 6.2f, zc}, {22.f, 1.f, zl}, wall);     // ceiling
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
    Vec3 plat{0.40f, 0.42f, 0.50f};
    add({0.f, 0.5f, 14.f}, {3.f, 1.f, 3.f}, plat);
    add({4.f, 1.2f, 10.f}, {2.f, 1.f, 2.f}, plat);
    add({1.f, 1.9f, 6.f}, {2.f, 1.f, 2.f}, plat);
    add({-3.f, 2.6f, 3.f}, {2.f, 1.f, 2.f}, plat);
    add({0.f, 3.3f, -1.f}, {3.f, 1.f, 3.f}, plat);
    return v;
}

struct AABB { Vec3 mn, mx; };
static bool aabbOverlap(Vec3 p, float r, float h, const AABB& b) {
    return p.x + r > b.mn.x && p.x - r < b.mx.x &&
           p.y + h > b.mn.y && p.y < b.mx.y &&
           p.z + r > b.mn.z && p.z - r < b.mx.z;
}

// A "monster" -- for now just a moving block that patrols and sends the player
// back to the start on contact.
struct Monster { Vec3 pos, vel, size; };

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

    GLuint sceneProg = link(VS, SCENE_FS);
    GLuint fogProg = link(FULL_VS, FOG_FS);
    GLuint compProg = link(FULL_VS, COMP_FS);

    // Scene program uniforms.
    GLint sMVP = glGetUniformLocation(sceneProg, "uMVP"), sModel = glGetUniformLocation(sceneProg, "uModel");
    GLint sLightDir = glGetUniformLocation(sceneProg, "uLightDir"), sCam = glGetUniformLocation(sceneProg, "uCam");
    GLint sTint = glGetUniformLocation(sceneProg, "uTint"), sNumL = glGetUniformLocation(sceneProg, "uNumLights");
    GLint sLPos = glGetUniformLocation(sceneProg, "uLightPos"), sLCol = glGetUniformLocation(sceneProg, "uLightCol");
    // Fog program uniforms.
    GLint fDepth = glGetUniformLocation(fogProg, "uDepth");
    GLint fCam = glGetUniformLocation(fogProg, "uCam"), fFwd = glGetUniformLocation(fogProg, "uFwd");
    GLint fRight = glGetUniformLocation(fogProg, "uRight"), fUp = glGetUniformLocation(fogProg, "uUp");
    GLint fTan = glGetUniformLocation(fogProg, "uTanHalf"), fAspect = glGetUniformLocation(fogProg, "uAspect");
    GLint fNear = glGetUniformLocation(fogProg, "uNear"), fFar = glGetUniformLocation(fogProg, "uFar");
    GLint fFogColor = glGetUniformLocation(fogProg, "uFogColor"), fTime = glGetUniformLocation(fogProg, "uTime");
    GLint fDensity = glGetUniformLocation(fogProg, "uFogDensity"), fHeight = glGetUniformLocation(fogProg, "uHeightFalloff");
    GLint fNoise = glGetUniformLocation(fogProg, "uNoiseScale"), fReg = glGetUniformLocation(fogProg, "uRegFogDensity");
    GLint fWind = glGetUniformLocation(fogProg, "uWindSpeed");
    GLint fSpotPos = glGetUniformLocation(fogProg, "uSpotPos"), fSpotDir = glGetUniformLocation(fogProg, "uSpotDir");
    GLint fSpotCos = glGetUniformLocation(fogProg, "uSpotCos"), fSpotCol = glGetUniformLocation(fogProg, "uSpotColor");
    GLint fSpotI = glGetUniformLocation(fogProg, "uSpotIntensity");
    GLint fNumL = glGetUniformLocation(fogProg, "uNumLights");
    GLint fLPos = glGetUniformLocation(fogProg, "uLightPos"), fLCol = glGetUniformLocation(fogProg, "uLightCol");
    // Composite uniforms.
    GLint cScene = glGetUniformLocation(compProg, "uScene"), cFog = glGetUniformLocation(compProg, "uFog");

    Mesh groundM = Mesh::plane(120.f, 60.f), boxM = Mesh::cube();
    GpuMesh ground = upload(groundM), box = upload(boxM);
    std::vector<BoxInst> scene = buildScene();
    std::vector<AABB> solids;
    for (const BoxInst& b : scene) {
        Vec3 h = b.size * 0.5f;
        solids.push_back({ {b.pos.x - h.x, b.pos.y - h.y, b.pos.z - h.z}, {b.pos.x + h.x, b.pos.y + h.y, b.pos.z + h.z} });
    }

    GLuint emptyVAO; glGenVertexArrays(1, &emptyVAO);

    // FBOs: scene (full res, colour + depth textures) and fog (low res, colour).
    GLuint sceneFBO = 0, sceneCol = 0, sceneDepth = 0, fogFBO = 0, fogCol = 0;
    int sW = 0, sH = 0, fW = 0, fH = 0;
    auto makeTex = [](GLuint& t, int w, int h, GLint ifmt, GLenum fmt, GLenum type, GLint filt) {
        glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    auto ensureScene = [&](int w, int h) {
        if (sceneFBO && sW == w && sH == h) return;
        if (sceneFBO) { glDeleteFramebuffers(1, &sceneFBO); glDeleteTextures(1, &sceneCol); glDeleteTextures(1, &sceneDepth); }
        sW = w; sH = h;
        makeTex(sceneCol, w, h, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, GL_NEAREST);
        makeTex(sceneDepth, w, h, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_NEAREST);
        glGenFramebuffers(1, &sceneFBO); glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneCol, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepth, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };
    auto ensureFog = [&](int w, int h) {
        if (fogFBO && fW == w && fH == h) return;
        if (fogFBO) { glDeleteFramebuffers(1, &fogFBO); glDeleteTextures(1, &fogCol); }
        fW = w; fH = h;
        makeTex(fogCol, w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
        glGenFramebuffers(1, &fogFBO); glBindFramebuffer(GL_FRAMEBUFFER, fogFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fogCol, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };

    Vec3 fogColor{0.62f, 0.63f, 0.66f};
    Vec3 ld = Vec3{-0.5f, -1.f, -0.4f}.normalized();
    const int NL = 4;
    float lpos[NL * 3] = { -6.f, 4.f, 12.f,   6.f, 4.f, -2.f,   0.f, 4.f, -16.f,  -1.f, 3.f, 5.f };
    float lcol[NL * 3] = { 1.0f, 0.7f, 0.4f,  0.5f, 0.7f, 1.0f, 1.0f, 0.8f, 0.5f, 0.7f, 0.85f, 1.0f };

    Vec3 pos{0.f, 0.f, 22.f};
    Vec3 vel{0.f, 0.f, 0.f};
    bool onGround = false;
    const Vec3 spawnPos{0.f, 0.f, 22.f};

    // Monsters: patrolling blocks. Touch one and you're sent back to the start.
    // Reaching the exit (far end) advances the level and adds another monster.
    std::vector<Monster> monsters;
    auto addMonster = [&](float x, float z, Vec3 v) { monsters.push_back({ {x, 0.9f, z}, v, {1.4f, 1.8f, 1.4f} }); };
    addMonster(-4.f, 2.f, {0.f, 0.f, 4.5f});
    addMonster(4.f, -10.f, {0.f, 0.f, -5.5f});
    addMonster(0.f, -22.f, {3.5f, 0.f, 0.f});
    int level = 1;
    float yaw = 3.14159265f, pitch = 0.f;
    float fogDensity = 0.6f, noiseScale = 0.31f, regFog = 0.028f, spotI = 0.7f, windSpeed = 2.5f;
    int fogScale = 2;                          // fog rendered at 1/fogScale resolution
    float t = 0.f;
    const float nearP = 0.1f, farP = 200.f, fovY = 60.f * 3.14159265f / 180.f;

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
                    case SDLK_f: spotI = (spotI > 0.f ? 0.f : 0.7f); break;
                    case SDLK_1: regFog = fmaxf(0.f, regFog - 0.004f); break;
                    case SDLK_2: regFog += 0.004f; break;
                    case SDLK_3: fogDensity = fmaxf(0.f, fogDensity - 0.05f); break;
                    case SDLK_4: fogDensity += 0.05f; break;
                    case SDLK_5: noiseScale = fmaxf(0.05f, noiseScale - 0.02f); break;
                    case SDLK_6: noiseScale += 0.02f; break;
                    case SDLK_7: windSpeed = fmaxf(0.f, windSpeed - 0.5f); break;
                    case SDLK_8: windSpeed += 0.5f; break;
                    case SDLK_SPACE: if (onGround) vel.y = 8.0f; break;
                    case SDLK_9: fogScale = 1; break;   // sharper fog
                    case SDLK_0: fogScale = 3; break;   // faster fog
                }
            }
        }

        Vec3 fwd{std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
        Vec3 fwdH = Vec3{fwd.x, 0.f, fwd.z}.normalized();
        Vec3 right = fwdH.cross(Vec3{0, 1, 0}).normalized();
        const Uint8* k = SDL_GetKeyboardState(nullptr);
        bool crouch = k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL];
        float eyeH = crouch ? 0.95f : 1.6f;
        float playerH = crouch ? 1.2f : 1.8f;
        const float rad = 0.3f;
        float spd = (k[SDL_SCANCODE_LSHIFT] ? 8.f : 4.5f) * (crouch ? 0.5f : 1.f);
        Vec3 wish{0, 0, 0};
        if (k[SDL_SCANCODE_W]) wish += fwdH;
        if (k[SDL_SCANCODE_S]) wish = wish - fwdH;
        if (k[SDL_SCANCODE_D]) wish += right;
        if (k[SDL_SCANCODE_A]) wish = wish - right;
        if (wish.length() > 0.001f) wish = wish.normalized();
        vel.x = wish.x * spd; vel.z = wish.z * spd;
        vel.y -= 24.f * dt;

        pos.x += vel.x * dt;
        for (const AABB& b : solids) if (aabbOverlap(pos, rad, playerH, b)) { pos.x = (vel.x > 0.f ? b.mn.x - rad : b.mx.x + rad); vel.x = 0.f; }
        pos.z += vel.z * dt;
        for (const AABB& b : solids) if (aabbOverlap(pos, rad, playerH, b)) { pos.z = (vel.z > 0.f ? b.mn.z - rad : b.mx.z + rad); vel.z = 0.f; }
        pos.y += vel.y * dt;
        onGround = false;
        for (const AABB& b : solids) if (aabbOverlap(pos, rad, playerH, b)) {
            if (vel.y <= 0.f) { pos.y = b.mx.y; onGround = true; } else { pos.y = b.mn.y - playerH; }
            vel.y = 0.f;
        }
        if (pos.y < 0.f) { pos.y = 0.f; vel.y = 0.f; onGround = true; }

        // Monsters move + bounce; touching one reverts you to the start.
        for (Monster& m : monsters) {
            m.pos = m.pos + m.vel * dt;
            if (m.pos.z > 26.f) { m.pos.z = 26.f; m.vel.z = -fabsf(m.vel.z); }
            if (m.pos.z < -36.f) { m.pos.z = -36.f; m.vel.z = fabsf(m.vel.z); }
            if (m.pos.x > 9.f) { m.pos.x = 9.f; m.vel.x = -fabsf(m.vel.x); }
            if (m.pos.x < -9.f) { m.pos.x = -9.f; m.vel.x = fabsf(m.vel.x); }
            Vec3 hs = m.size * 0.5f;
            AABB mb{ {m.pos.x - hs.x, m.pos.y - hs.y, m.pos.z - hs.z}, {m.pos.x + hs.x, m.pos.y + hs.y, m.pos.z + hs.z} };
            if (aabbOverlap(pos, rad, playerH, mb)) { pos = spawnPos; vel = Vec3{0, 0, 0}; }
        }
        // Reached the exit at the far end? Escaped -> next level, one more monster.
        if (pos.z < -34.f && fabsf(pos.x) < 3.f) {
            ++level;
            addMonster(level % 2 ? -6.f : 6.f, 0.f, {0.f, 0.f, level % 2 ? 5.f : -5.f});
            pos = spawnPos; vel = Vec3{0, 0, 0};
        }

        Vec3 eye = pos + Vec3{0.f, eyeH, 0.f};

        ensureScene(W, H);
        int fw = (W / fogScale < 1) ? 1 : W / fogScale;
        int fh = (H / fogScale < 1) ? 1 : H / fogScale;
        ensureFog(fw, fh);

        Mat4 proj = perspective(fovY, float(W) / float(H), nearP, farP);
        Mat4 view = lookAt(eye, eye + fwd, Vec3{0, 1, 0});

        // ---- 1) Geometry pass (full res, crisp) ----
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
        glViewport(0, 0, W, H);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
        glClearColor(fogColor.x, fogColor.y, fogColor.z, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(sceneProg);
        glUniform3f(sLightDir, ld.x, ld.y, ld.z);
        glUniform3f(sCam, eye.x, eye.y, eye.z);
        glUniform1i(sNumL, NL);
        glUniform3fv(sLPos, NL, lpos);
        glUniform3fv(sLCol, NL, lcol);
        auto drawGeo = [&](const GpuMesh& o, const Mat4& model, Vec3 tint) {
            Mat4 mvp = proj * view * model;
            glUniformMatrix4fv(sMVP, 1, GL_TRUE, &mvp.m[0][0]);
            glUniformMatrix4fv(sModel, 1, GL_TRUE, &model.m[0][0]);
            glUniform3f(sTint, tint.x, tint.y, tint.z);
            glBindVertexArray(o.vao);
            glDrawArrays(GL_TRIANGLES, 0, o.count);
        };
        drawGeo(ground, translate({0, 0, 0}), {0.42f, 0.44f, 0.46f});
        for (const BoxInst& b : scene) drawGeo(box, translate(b.pos) * rotateY(b.rotY) * scale(b.size), b.tint);
        for (const Monster& m : monsters) drawGeo(box, translate(m.pos) * scale(m.size), {0.85f, 0.15f, 0.13f});
        drawGeo(box, translate({0.f, 1.2f, -36.f}) * scale({4.f, 2.4f, 0.4f}), {0.2f, 0.9f, 0.35f});  // exit

        // ---- 2) Fog pass (low res) ----
        Vec3 camRight = fwd.cross(Vec3{0, 1, 0}).normalized();
        Vec3 camUp = camRight.cross(fwd).normalized();
        glBindFramebuffer(GL_FRAMEBUFFER, fogFBO);
        glViewport(0, 0, fw, fh);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(fogProg);
        pglActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneDepth);
        glUniform1i(fDepth, 0);
        glUniform3f(fCam, eye.x, eye.y, eye.z);
        glUniform3f(fFwd, fwd.x, fwd.y, fwd.z);
        glUniform3f(fRight, camRight.x, camRight.y, camRight.z);
        glUniform3f(fUp, camUp.x, camUp.y, camUp.z);
        glUniform1f(fTan, std::tan(fovY * 0.5f));
        glUniform1f(fAspect, float(W) / float(H));
        glUniform1f(fNear, nearP); glUniform1f(fFar, farP);
        glUniform3f(fFogColor, fogColor.x, fogColor.y, fogColor.z);
        glUniform1f(fTime, t);
        glUniform1f(fDensity, fogDensity);
        glUniform1f(fHeight, 0.30f);
        glUniform1f(fNoise, noiseScale);
        glUniform1f(fReg, regFog);
        glUniform1f(fWind, windSpeed);
        glUniform3f(fSpotPos, eye.x, eye.y, eye.z);
        glUniform3f(fSpotDir, fwd.x, fwd.y, fwd.z);
        glUniform1f(fSpotCos, std::cos(22.f * 3.14159265f / 180.f));
        glUniform3f(fSpotCol, 1.0f, 0.95f, 0.82f);
        glUniform1f(fSpotI, spotI);
        glUniform1i(fNumL, NL);
        glUniform3fv(fLPos, NL, lpos);
        glUniform3fv(fLCol, NL, lcol);
        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // ---- 3) Composite to the screen (full res) ----
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, W, H);
        glUseProgram(compProg);
        pglActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneCol); glUniform1i(cScene, 0);
        pglActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, fogCol); glUniform1i(cFog, 1);
        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
