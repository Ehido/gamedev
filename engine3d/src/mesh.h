#pragma once
#include <string>
#include <vector>
#include "swrender.h"

// A mesh is just a triangle list (every 3 vertices = 1 triangle), with UVs.
struct Mesh {
    std::vector<Vertex> verts;

    static Mesh cube();
    static Mesh uvSphere(int rings, int sectors);
    static Mesh plane(float size, float uvRepeat);
    bool loadObj(const std::string& path);  // v / vt / f (triangulated)
};
