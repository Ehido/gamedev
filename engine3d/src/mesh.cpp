#include "mesh.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

Mesh Mesh::cube() {
    // 6 faces, each a quad (2 tris) with 0..1 UVs.
    const Vec3 corners[8] = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
    const int faces[6][4] = {
        {4, 5, 6, 7},  // +z
        {1, 0, 3, 2},  // -z
        {0, 4, 7, 3},  // -x
        {5, 1, 2, 6},  // +x
        {7, 6, 2, 3},  // +y
        {0, 1, 5, 4},  // -y
    };
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    Mesh m;
    for (auto& f : faces) {
        Vertex v0{corners[f[0]], uv[0][0], uv[0][1]};
        Vertex v1{corners[f[1]], uv[1][0], uv[1][1]};
        Vertex v2{corners[f[2]], uv[2][0], uv[2][1]};
        Vertex v3{corners[f[3]], uv[3][0], uv[3][1]};
        m.verts.push_back(v0); m.verts.push_back(v1); m.verts.push_back(v2);
        m.verts.push_back(v0); m.verts.push_back(v2); m.verts.push_back(v3);
    }
    return m;
}

Mesh Mesh::uvSphere(int rings, int sectors) {
    Mesh m;
    auto at = [&](int r, int s) -> Vertex {
        float fr = float(r) / rings;        // 0..1 top->bottom
        float fs = float(s) / sectors;      // 0..1 around
        float theta = fr * 3.14159265f;     // polar
        float phi = fs * 2.f * 3.14159265f; // azimuth
        Vec3 p{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
        return {p * 0.5f, fs, fr};
    };
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < sectors; ++s) {
            Vertex a = at(r, s), b = at(r + 1, s), c = at(r + 1, s + 1), d = at(r, s + 1);
            m.verts.push_back(a); m.verts.push_back(b); m.verts.push_back(c);
            m.verts.push_back(a); m.verts.push_back(c); m.verts.push_back(d);
        }
    return m;
}

Mesh Mesh::plane(float size, float uvRepeat) {
    Mesh m;
    float h = size * 0.5f;
    Vertex a{{-h, 0.f, -h}, 0.f, 0.f};
    Vertex b{{h, 0.f, -h}, uvRepeat, 0.f};
    Vertex c{{h, 0.f, h}, uvRepeat, uvRepeat};
    Vertex d{{-h, 0.f, h}, 0.f, uvRepeat};
    m.verts = {a, b, c, a, c, d};
    return m;
}

bool Mesh::loadObj(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::vector<Vec3> pos;
    std::vector<std::pair<float, float>> uvs;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            Vec3 p;
            ss >> p.x >> p.y >> p.z;
            pos.push_back(p);
        } else if (tag == "vt") {
            float u = 0, v = 0;
            ss >> u >> v;
            uvs.push_back({u, v});
        } else if (tag == "f") {
            std::vector<Vertex> face;
            std::string tok;
            while (ss >> tok) {
                int vi = 0, ti = 0;
                // formats: v, v/vt, v/vt/vn, v//vn
                std::sscanf(tok.c_str(), "%d/%d", &vi, &ti);
                if (vi < 0) vi = int(pos.size()) + 1 + vi;
                Vertex vert;
                if (vi >= 1 && vi <= int(pos.size())) vert.pos = pos[vi - 1];
                if (ti >= 1 && ti <= int(uvs.size())) {
                    vert.u = uvs[ti - 1].first;
                    vert.v = uvs[ti - 1].second;
                }
                face.push_back(vert);
            }
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                verts.push_back(face[0]);
                verts.push_back(face[i]);
                verts.push_back(face[i + 1]);
            }
        }
    }
    return !verts.empty();
}
