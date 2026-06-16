# MURK — design & art direction (working title)

A first-person co-op scavenger game where **the fog is the enemy and the light
is your lifeline**. You go into places the murk has swallowed, grab what's
valuable, and get out before you lose your way — or your nerve. Stylized, not
realistic. Built in **Godot 4**.

> MURK is the 3D continuation of the loop we prototyped in `../haul` (grab loot
> → weight slows you → extract → bank cash → push your luck). The mechanics
> carry over; the presentation is the whole point now.

## The hook: difficulty = visibility
Most games make "hard" mean enemies with more health. MURK makes **hard mean you
can see less.** Difficulty is a single dial that moves *atmosphere*:

| Difficulty | Fog | Flashlight reach | Feel |
|-----------|-----|------------------|------|
| Easy      | thin | far (~22m) | you can plan, breathe |
| Normal    | medium | ~15m | tense |
| Hard      | thick | ~11m | claustrophobic |
| Nightmare | soup | ~8m | you can barely see your hands |

All of this lives in `scripts/Difficulty.gd` as presets, applied live — press
**TAB** in the prototype to feel the room close in around you. This is the thing
to nail first, and it's already wired.

## The look
- **Stylized, atmospheric, NOT realistic.** Bold, simple shapes lost in fog;
  the mood comes from light + murk, not detailed textures.
- **Drifting volumetric fog.** The fog moves and breathes — animated 3D noise
  drives its density (`shaders/moving_fog.gdshader`), so it rolls past you
  instead of sitting still. Difficulty sets how thick it gets.
- **The flashlight is the star.** A shadow-casting spotlight that carves a
  visible cone through the volumetric fog (god-ray shafts). Your pool of light
  is safety; everything past it is unknown.
- **Planned next:** a PSX/posterize post-process pass (dither + color crunch +
  grain) for a cohesive lo-fi identity, plus flashlight flicker and a battery.

## Story (first draft — to shape together)
You're a **ferryman**: a salvager the Lighthouse hires to go into the drowned
places — towns, rigs, whole districts the murk rolled over and never gave back.
The Lighthouse keeps a beam burning out at the edge of the fog; as long as you
keep bringing returns, it keeps the light on for you. Stop being profitable and
the light goes dark — and nobody walks out of the murk without it.

The longer you stay and the greedier you get, the thicker the fog grows and the
more you hear *other* things moving in it — the crews who came before, and
whatever they became. Story is told through **salvaged notes and crackling
radio** from the Lighthouse and lost ferrymen, not cutscenes.

Why it fits the mechanics:
- **Fog thickening with time/greed** = the danger meter, now literally the air.
- **Difficulty = how far the Lighthouse's light reaches you** = the visibility dial.
- **Greed vs. extraction** = stay for one more haul, or make it back to the light.

## Status of this prototype
Walkable first-person test room: drifting volumetric fog, a flashlight cutting
through it, and the four difficulty presets switchable live (TAB). No loot,
enemies, or extraction yet — this build exists to lock the **look and feel of
the fog + light** first, per the look-first plan. Built programmatically in
`scripts/Main.gd` so it's easy to iterate from screenshots.

## Roadmap
1. **Look & feel (now):** fog + flashlight + difficulty — confirm it looks good.
2. PSX/stylized post-process pass; flashlight flicker + battery.
3. Loot + weight + extraction point (port the HAUL loop into 3D).
4. A threat that lives in the fog (audio-first, sees you by your light).
5. Co-op + **in-game proximity voice** (the payoff).
6. Story layer: notes, radio, the Lighthouse.
