#pragma once

// All gameplay tuning lives here so feel can be adjusted without touching
// engine or logic code. Speeds are in pixels/second; weights/values are
// abstract units.
namespace haul {
namespace cfg {

constexpr float ArenaMargin     = 24.f;
constexpr float PlayerBaseSpeed = 240.f;  // speed when carrying nothing
constexpr float PlayerSize      = 22.f;
constexpr float PickupRadius    = 30.f;   // how close to grab loot

// Weight -> speed. Effective speed = base * weightPenalty(weight, MaxWeight).
constexpr float MaxWeight       = 100.f;  // "capacity" before max slowdown
constexpr float MinSpeedFactor  = 0.34f;  // slowest you move at/over capacity
constexpr float WeightSlowdown  = 0.72f;  // how hard weight bites

constexpr int   LootCount       = 14;

// Loot tiers: value (cash), weight, RGB color.
struct Tier { int value; float weight; unsigned char r, g, b; };
constexpr Tier Tiers[3] = {
    { 10,  8.f, 120, 200, 255},  // common  (blue)
    { 30, 20.f, 185, 140, 255},  // rare    (purple)
    { 80, 44.f, 255, 205,  90},  // jackpot (gold)
};

// Returns a 0..1 speed multiplier based on how loaded the player is.
inline float weightPenalty(float weight) {
    float over = (MaxWeight > 0.f) ? weight / MaxWeight : 0.f;
    float f = 1.f - over * WeightSlowdown;
    return f < MinSpeedFactor ? MinSpeedFactor : f;
}

} // namespace cfg
} // namespace haul
