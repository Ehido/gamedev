# MURK (prototype)

A first-person fog-and-flashlight scavenger game in **Godot 4.3**. See
[DESIGN.md](DESIGN.md) for the concept, art direction, and story.

## Run it

> **You have to run this yourself to see it.** It was authored in a headless
> cloud environment with no GPU, so the renders (fog, light, the whole look)
> can only be seen on your machine. Open it, then send screenshots back and
> we iterate.

1. Install **Godot 4.3** (standard, not .NET): https://godotengine.org/download
2. Open Godot → **Import** → select `gamedev/games/murk/project.godot`.
3. Press **Play** (F5).

Or from the command line:

```sh
godot --path gamedev/games/murk
```

## Controls

| Key | Action |
|-----|--------|
| **WASD** | Move |
| **Mouse** | Look |
| **TAB** | Cycle difficulty (Easy → Normal → Hard → Nightmare) — watch the fog thicken and the flashlight shrink |
| **Esc** | Free the mouse cursor |

## What to look at / tell me

This build is about the **fog + light feel**, nothing else yet. When you run it:
- Does the drifting fog look good, or too thick / thin / flat?
- Does the flashlight cut a nice cone through the fog (visible shafts)?
- Does cycling difficulty (TAB) *feel* like the world is closing in?

Your screenshots + reactions drive the next iteration.

## Note on requirements

Volumetric fog needs the **Forward+** renderer (the default) and a GPU that
supports Vulkan. If the fog doesn't show, check Project Settings → Rendering →
Renderer is `Forward+`.
