# engine3d — our from-scratch 3D engine

A 3D engine built from the ground up in C++. We chose to build our own rather
than use Godot; this is where that starts.

## Why a software renderer first
The build environment has **no GPU and no display**, so a Vulkan/OpenGL renderer
can't run or be seen here. A **software (CPU) rasterizer runs anywhere and
renders to a PNG**, so we can build and *see* progress immediately. It's not a
throwaway — the CPU pipeline (math, transforms, rasterization, depth buffer,
shading) is the real foundation of any 3D engine. A GPU backend for real-time
speed gets added once the architecture is solid.

## What works (Milestone 0)
- `math3d.h` — vectors + a 4x4 matrix with translate/scale/rotate/perspective/lookAt.
- `swrender.{h,cpp}` — colour + depth framebuffer, a triangle rasteriser with
  perspective projection, **z-buffer**, **flat Lambert shading** with a
  directional light, and **distance fog**. Saves to PNG.
- `main.cpp` — a scene of lit, fogged cubes rendered by the above.

## Build & run

```sh
sudo apt-get install -y build-essential cmake libsdl2-dev libsdl2-image-dev
cmake -S gamedev/engine3d -B gamedev/engine3d/build
cmake --build gamedev/engine3d/build -j
./gamedev/engine3d/build/engine3d out.png     # renders a frame to out.png
```
(SDL2 is used only to write the PNG; all rendering is our own code.)

## Roadmap
1. **M0 (done):** software rasteriser — perspective, z-buffer, shading, fog.
2. Perspective-correct interpolation, near-plane clipping, textures, `.obj` loading.
3. A scene graph + camera controller; render animation frames.
4. **GPU backend (OpenGL):** real windowing + real-time speed on a real machine —
   this is where it becomes interactive and you run it yourself.
5. Re-implement the MURK look (volumetric-ish fog, dynamic lights) on our engine.

The Godot MURK build stays in `../games/murk` as the design/visual target we're
rebuilding toward.
