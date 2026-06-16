# MURK — idea backlog (captured, not yet built)

Running list of directions to consider later. Nothing here is implemented yet —
it's here so we don't lose the ideas while we focus on the core game.

## Custom Game mode + map editor
- A **map editor** where players design their own layouts. Devs (us) also ship
  curated maps over time.
- A **Custom Game** mode where the host can change **almost every setting** —
  fog density, light reach, enemy count/speed, loot value, timers, etc. (We
  already have the data-driven `Difficulty` presets; a custom mode would expose
  those values as live sliders instead of fixed tiers — good foundation.)
- Worth doing *after* the core loop + multiplayer are solid, since a sandbox is
  only fun once there's a game to sandbox.

## Monetization / microtransactions
- Captured per request. Strong recommendation when we get there:
  **cosmetic-only** (skins, flashlight colors, voice filters, map-editor props).
  Co-op horror communities punish pay-to-win hard; cosmetics + optional map/DLC
  packs are the safe, well-liked model (cf. how the genre's hits monetize).

## Item tiers (loot economy)
- Yes to tiered loot (common → rare → jackpot), carried over from the HAUL
  prototype: each item has cash **value + weight**. Tiers drive the greed/risk
  decision (grab the heavy jackpot deep in the fog, or play it safe?).
- **"Won't this become a knockoff Phasmophobia?"** — No, because the *core loop
  is different*. Phasmophobia is an **investigation** game (use tools to deduce
  which ghost it is). MURK is a **greed/extraction** game (grab valuables, weight
  slows you, haul to extraction before the fog/threat gets you). Same spooky
  surface, opposite verbs. Our identity = REPO/Lethal-style risk-greed in
  dynamic fog with visibility-as-difficulty, not ghost identification. Keeping
  the loop about *hauling loot out under pressure* (not *identifying a monster*)
  is what keeps us distinct.

## Settings / accessibility
- The community-brightness-mod problem (too-dark games) is a real one — our
  difficulty deliberately keeps the flashlight a viable lifeline. A future
  options menu should also expose a brightness/gamma slider.

## Character face: eyes lead, head follows ("side-eye")
- The character's **eyes dart toward where you look BEFORE the head turns**, then
  the head catches up. Reads as alive and is very clippable — in co-op, teammates
  see your face react.
- **DPI/controller-independent:** normalise the look delta and clamp the eye/head
  follow to a MAX rate, so a fast flick always looks like "eyes lead, head
  follows" regardless of mouse DPI or stick sensitivity (never an instant snap).
  This clamp applies to the *face animation*, not the camera aim (aim stays 1:1).
- **Needs a character model** with a riggable face + co-op to be visible (first
  person never sees its own face). Build the model first; the look-tracking math
  can be added then.

## Engine: custom vs Godot (revisited)
- Considered building our own engine again. The reality is unchanged: a
  from-scratch 3D engine that matches what we ALREADY have for free in Godot
  (volumetric fog, PBR, real-time shadows, SDFGI, bloom, the lava shader) is a
  multi-year, multi-person effort — we'd spend years rebuilding to get back to
  where we are today. Godot is extensible exactly where "make your own" matters:
  custom shaders (already doing it) and C++/GDExtension for perf-critical
  systems. Recommendation: stay on Godot; get custom-engine benefits on top of it.
