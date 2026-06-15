# gamedev

A from-scratch C++ game engine and the games built on it. Lives alongside the
unrelated data-scraper in this repo but is fully self-contained.

## Layout

```
gamedev/
├── engine/          # reusable engine (game loop, renderer, input, fonts, math)
│   ├── include/engine/   public headers
│   └── src/              implementation
└── games/
    └── haul/        # HAUL — a co-op scavenger game (work in progress)
```

The engine is a thin layer over **SDL2** (window, input, audio, GPU access) with
everything above it — the fixed-timestep game loop, rendering abstraction,
entities, math — written from scratch. The fixed timestep is deliberate: it
keeps the simulation deterministic, which is the foundation for networked
multiplayer + proximity voice later.

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

cmake -S gamedev -B gamedev/build
cmake --build gamedev/build -j
./gamedev/build/games/haul/haul
```

Controls: **WASD / arrows** to move, walk over loot to grab it, stand on the
green **EXTRACT** zone to bank it, **Esc** to quit.

### Headless / screenshots

The engine can render offscreen (no display needed):

```sh
./gamedev/build/games/haul/haul --screenshot frame.png   # one frame to PNG
./gamedev/build/games/haul/haul --demo --screenshot f.png # let a bot play first
```
