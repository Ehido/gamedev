# engine3d — our from-scratch 3D engine

A 3D engine built from the ground up in C++, with **two render backends**:

- **Software renderer** (`engine3d`) — our own CPU rasterizer. Runs anywhere
  (no GPU), so progress is always visible as PNGs. It's the foundation: 3D math,
  transforms, clipping, rasterization, depth buffer, texturing, shading, fog.
- **GPU backend** (`engine3d_gl`) — the same scene through **real OpenGL
  (GLES 3.0)** via EGL, rendering offscreen. This is the path to real-time,
  interactive speed on a real machine. Validated here headless through Mesa's
  software driver (llvmpipe).

## What works

- `math3d.h` — vectors + 4x4 matrix (translate/scale/rotate/perspective/lookAt).
- `swrender.{h,cpp}` — CPU rasterizer: **near-plane clipping**,
  **perspective-correct** UV interpolation, **z-buffer**, texture sampling, flat
  Lambert shading, distance fog, PNG output.
- `mesh.{h,cpp}` — cube / UV-sphere / plane primitives + a Wavefront **`.obj`
  loader** (see `assets/octahedron.obj`).
- `gl_main.cpp` — EGL surfaceless GLES3 backend: offscreen FBO, GLSL shaders
  (per-fragment lighting + fog), VAO/VBO mesh upload, `glReadPixels` → PNG.

## Build & run

```sh
sudo apt-get install -y build-essential cmake \
    libsdl2-dev libsdl2-image-dev libgles-dev libegl-dev mesa-libgallium ffmpeg
cmake -S gamedev/engine3d -B gamedev/engine3d/build
cmake --build gamedev/engine3d/build -j

# software renderer: a single textured/fogged frame
./gamedev/engine3d/build/engine3d --assets gamedev/engine3d/assets out.png

# software renderer: an orbit animation -> GIF
mkdir -p frames
./gamedev/engine3d/build/engine3d --assets gamedev/engine3d/assets --frames 48 --outdir frames
ffmpeg -framerate 24 -i frames/frame_%03d.png anim.gif

# GPU backend (OpenGL ES 3.0, offscreen). On a headless box it uses Mesa's
# software driver; on a real machine it uses the GPU.
./gamedev/engine3d/build/engine3d_gl gamedev/engine3d/assets gl_out.png
```

## Roadmap
1. ~~M0: software rasterizer — perspective, z-buffer, shading, fog.~~ done
2. ~~Near-plane clipping, perspective-correct interpolation, textures, OBJ.~~ done
3. ~~Animation (orbiting camera -> frame sequence -> GIF).~~ done
4. ~~GPU backend (OpenGL ES via EGL), validated headless.~~ done
5. **Next:** real windowing + input on the GPU backend (interactive, runs on
   your machine), then a scene/asset format, then rebuild the MURK look.

The Godot MURK build stays in `../games/murk` as the visual target.
