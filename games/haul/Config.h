#pragma once

// All gameplay tuning lives here so feel can be adjusted without touching
// engine or logic code. Speeds are in pixels/second; weights/values/health
// are abstract units.
namespace haul {
namespace cfg {

constexpr float ArenaMargin     = 24.f;
constexpr float PlayerBaseSpeed = 240.f;  // speed when carrying nothing
constexpr float PlayerSize      = 22.f;
constexpr float PickupRadius    = 30.f;   // how close to grab loot
constexpr float PlayerMaxHealth = 100.f;
constexpr float IFrameSeconds   = 0.7f;   // invulnerability after a hit

// Weight -> speed. Effective speed = base * weightPenalty(weight).
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

// Enemies (roamers): wander until the player is near, then chase.
constexpr float RoamerSize       = 28.f;
constexpr float RoamerChaseSpeed = 135.f;
constexpr float RoamerWanderSpeed = 64.f;
constexpr float RoamerAggroRange = 200.f;  // distance at which they notice you
constexpr float RoamerDamage     = 24.f;
constexpr int   RoamerCountBase  = 2;      // enemies present at run start

// Danger meter: rises over time, escalating the threat to push extraction.
constexpr float DangerRisePerSec  = 0.02f;  // fills 0->1 in ~50s
constexpr float DangerSpeedBonus  = 0.6f;   // +60% enemy speed at max danger
constexpr float DangerThresholds[3] = {0.35f, 0.60f, 0.85f};  // spawn points

// Returns a 0..1 speed multiplier based on how loaded the player is.
inline float weightPenalty(float weight) {
    float over = (MaxWeight > 0.f) ? weight / MaxWeight : 0.f;
    float f = 1.f - over * WeightSlowdown;
    return f < MinSpeedFactor ? MinSpeedFactor : f;
}

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace cfg
} // namespace haul
