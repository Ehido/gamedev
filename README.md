# gamedev

A from-scratch C++ game engine (2D **and** 3D), plus the games and prototypes
built on it. Extracted into its own repo from the Velocitry-data experiment.

## Layout

```
engine/      # 2D engine — game loop, renderer, input, fonts, math (thin layer over SDL2)
engine3d/    # 3D engine — software rasteriser + OpenGL ES backend, volumetric fog, first-person walk
games/
├── haul/    # HAUL — a co-op scavenger game (C++/SDL2)
└── murk/    # MURK — a 3D fog-scavenger built in Godot 4
```

## Downloads

Playable builds and demo clips are published on the
[Releases page](https://github.com/Ehido/gamedev/releases/latest):

- **MURK.exe** — the Godot MURK build (Windows)
- **MURK_walk.zip** — the engine3d first-person fog walkthrough (Windows)
- Demo GIFs of the engine3d volumetric fog, light shafts, and scene flythrough

Prebuilt binaries are intentionally **not** committed to the repo — they live as
Release assets so the source tree stays small.

## Screenshots

Stills live in [`engine3d/renders/`](engine3d/renders) — software vs OpenGL
scenes, the volumetric-fog variation grid, and light-shaft frames.

## HAUL

A scavenger game about greed vs. danger: explore an arena, grab loot (each item
has a cash **value** and a **weight**), haul it to the extraction zone to bank
it as cash — but the heavier you carry, the slower you move. Danger and a shop
come in later milestones.

### Build & run (Linux)

Requires a C++17 compiler, CMake, and SDL2 + SDL2_image + SDL2_ttf dev packages:

```sh
sudo apt-get install -y build-essential cmake \
    libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev

cmake -S . -B build
cmake --build build -j
./build/games/haul/haul
```

Controls: **WASD / arrows** to move, walk over loot to grab it, stand on the
green **EXTRACT** zone to bank it, **Esc** to quit.

### Headless / screenshots

The engine can render offscreen (no display needed):

```sh
./build/games/haul/haul --screenshot frame.png       # one frame to PNG
./build/games/haul/haul --demo --screenshot f.png    # let a bot play first
./build/games/haul/haul --demo --frames 600 --screenshot f.png  # bot runs N steps
./build/games/haul/haul --faces --screenshot f.png   # the reactive-face gallery
```

The character has a reactive face (a portrait in the top-right corner) that
emotes from live game state: greedy near loot, scared/panicking when chased,
cha-ching on a deposit, dead on game over.
