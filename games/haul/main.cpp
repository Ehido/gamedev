// HAUL (working title) -- a co-op scavenger game: grab loot, weight slows you
// down, haul it to the extraction zone, bank it for cash, push your luck.
//
// Milestone 2: DANGER. Roaming enemies wander the arena and chase when you get
// close, dealing contact damage. Run out of health and it's game over -- you
// forfeit the loot you were carrying, but banked cash is safe. A danger meter
// climbs over time, spawning more (and faster) enemies, so lingering for "one
// more" item gets riskier the longer you stay.
//
// Flags: `--screenshot out.png` renders one frame headless; `--demo` lets a bot
// play (it flees nearby enemies); `--frames N` sets how long the bot runs first.

#include "engine/App.h"
#include "engine/Game.h"
#include "engine/Renderer.h"
#include "engine/Input.h"
#include "engine/Font.h"
#include "engine/Vec2.h"
#include "engine/Color.h"
#include "Config.h"
#include "Face.h"

#include <SDL.h>
#include <cmath>
#include <string>
#include <vector>

using namespace eng;
namespace cfg = haul::cfg;
using haul::FaceState;
using haul::drawFace;
using haul::faceLabel;

namespace {

struct LootItem {
    Vec2 pos;
    Vec2 size;
    int value;
    float weight;
    Color color;
    bool collected = false;
};

struct Roamer {
    Vec2 pos;
    Vec2 heading{1.f, 0.f};
    float wanderTimer = 0.f;
    bool aggro = false;
};

bool loadHudFont(Font& font, int pt) {
    std::vector<std::string> paths;
    if (char* base = SDL_GetBasePath()) {
        paths.push_back(std::string(base) + "assets/DejaVuSans.ttf");
        SDL_free(base);
    }
    paths.push_back("assets/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
    for (const auto& p : paths) {
        if (font.load(p, pt)) return true;
    }
    return false;
}

} // namespace

class HaulGame : public Game {
public:
    void setAutoplay(bool b) { autoplay_ = b; }
    void setGallery(bool b) { gallery_ = b; }

    void init(Renderer& r) override {
        arenaW_ = static_cast<float>(r.width());
        arenaH_ = static_cast<float>(r.height());
        zone_ = {cfg::ArenaMargin + 6.f, arenaH_ - cfg::ArenaMargin - 132.f, 152.f, 126.f};

        loadHudFont(font_, 18);
        loadHudFont(bigFont_, 30);

        rng_ = 20260615u;  // deterministic so the first screenshot is stable
        startRun();
    }

    void update(float dt, const Input& in) override {
        if (gameOver_) {
            if (in.down(SDL_SCANCODE_R) || in.down(SDL_SCANCODE_RETURN)) startRun();
            if (in.down(SDL_SCANCODE_ESCAPE)) quit_ = true;
            return;
        }

        // --- movement (weight-penalized) ---
        Vec2 dir = autoplay_ ? autopilotDir() : inputDir(in);
        if (dir.x != 0.f || dir.y != 0.f) {
            float speed = cfg::PlayerBaseSpeed * cfg::weightPenalty(carriedWeight_);
            player_ += dir.normalized() * (speed * dt);
        }
        const float lo = cfg::ArenaMargin + cfg::PlayerSize * 0.5f;
        player_.x = cfg::clampf(player_.x, lo, arenaW_ - lo);
        player_.y = cfg::clampf(player_.y, lo, arenaH_ - lo);

        // --- pickup ---
        for (auto& l : loot_) {
            if (l.collected) continue;
            if ((player_ - l.pos).length() <= cfg::PickupRadius + l.size.x * 0.5f) {
                l.collected = true;
                carriedValue_ += l.value;
                carriedWeight_ += l.weight;
            }
        }

        // --- banking ---
        bool inZone = player_.x >= zone_.x && player_.x <= zone_.x + zone_.w &&
                      player_.y >= zone_.y && player_.y <= zone_.y + zone_.h;
        if (inZone && carriedValue_ > 0) {
            cash_ += carriedValue_;
            carriedValue_ = 0;
            carriedWeight_ = 0.f;
            bankFlash_ = 0.6f;
        }
        if (bankFlash_ > 0.f) bankFlash_ -= dt;

        // --- danger meter + escalating spawns ---
        danger_ = cfg::clampf(danger_ + cfg::DangerRisePerSec * dt, 0.f, 1.f);
        while (nextThreshold_ < 3 && danger_ >= cfg::DangerThresholds[nextThreshold_]) {
            spawnRoamer();
            ++nextThreshold_;
        }

        // --- enemies ---
        for (auto& e : enemies_) updateRoamer(e, dt);

        // --- damage ---
        if (iframe_ > 0.f) iframe_ -= dt;
        for (const auto& e : enemies_) {
            bool hit = std::fabs(e.pos.x - player_.x) < (cfg::PlayerSize + cfg::RoamerSize) * 0.5f &&
                       std::fabs(e.pos.y - player_.y) < (cfg::PlayerSize + cfg::RoamerSize) * 0.5f;
            if (hit && iframe_ <= 0.f) {
                health_ -= cfg::RoamerDamage;
                iframe_ = cfg::IFrameSeconds;
                if (health_ <= 0.f) {
                    health_ = 0.f;
                    gameOver_ = true;          // forfeit carried loot; cash kept
                    carriedValue_ = 0;
                    carriedWeight_ = 0.f;
                }
                break;
            }
        }

        if (in.down(SDL_SCANCODE_ESCAPE)) quit_ = true;
    }

    void render(Renderer& r) override {
        if (gallery_) { renderGallery(r); return; }

        r.clear({18, 18, 26, 255});

        bool inZone = player_.x >= zone_.x && player_.x <= zone_.x + zone_.w &&
                      player_.y >= zone_.y && player_.y <= zone_.y + zone_.h;

        r.outlineRect(cfg::ArenaMargin, cfg::ArenaMargin,
                      arenaW_ - 2 * cfg::ArenaMargin, arenaH_ - 2 * cfg::ArenaMargin,
                      {64, 64, 86, 255}, 2);

        Color zoneFill = (inZone || bankFlash_ > 0.f) ? Color{36, 92, 56, 255}
                                                       : Color{26, 60, 40, 255};
        r.fillRect(zone_.x, zone_.y, zone_.w, zone_.h, zoneFill);
        r.outlineRect(zone_.x, zone_.y, zone_.w, zone_.h, {90, 220, 130, 255}, 3);
        if (font_.valid()) {
            font_.draw(r, "EXTRACT", zone_.x + 26.f, zone_.y + zone_.h * 0.5f - 10.f,
                       {150, 240, 180, 255});
        }

        for (const auto& l : loot_) {
            if (!l.collected) r.fillRectCentered(l.pos, l.size, l.color);
        }

        // Enemies: crimson body, yellow warning outline (brighter when chasing).
        for (const auto& e : enemies_) {
            r.fillRectCentered(e.pos, {cfg::RoamerSize, cfg::RoamerSize}, {200, 40, 52, 255});
            Color outline = e.aggro ? Color{255, 230, 60, 255} : Color{150, 120, 40, 255};
            r.outlineRect(e.pos.x - cfg::RoamerSize * 0.5f, e.pos.y - cfg::RoamerSize * 0.5f,
                          cfg::RoamerSize, cfg::RoamerSize, outline, 3);
        }

        // Player: tints toward heavy/orange as load grows; flashes during i-frames.
        float load = cfg::clampf(carriedWeight_ / cfg::MaxWeight, 0.f, 1.f);
        Color pcol{static_cast<unsigned char>(70 + 150 * (1.f - load)),
                   static_cast<unsigned char>(170 - 90 * load),
                   static_cast<unsigned char>(230 - 120 * load), 255};
        bool blink = iframe_ > 0.f && (static_cast<int>(iframe_ * 20.f) % 2 == 0);
        if (!blink) {
            r.fillRectCentered(player_, {cfg::PlayerSize, cfg::PlayerSize}, pcol);
            r.outlineRect(player_.x - cfg::PlayerSize * 0.5f, player_.y - cfg::PlayerSize * 0.5f,
                          cfg::PlayerSize, cfg::PlayerSize, {255, 255, 255, 255}, 2);
        }

        drawHud(r);

        if (gameOver_) {
            r.fillRect(0, 0, arenaW_, arenaH_, {0, 0, 0, 175});
            if (bigFont_.valid())
                bigFont_.draw(r, "YOU DIED", arenaW_ * 0.5f - 78.f, arenaH_ * 0.5f - 60.f,
                              {255, 90, 90, 255});
            if (font_.valid()) {
                font_.draw(r, "Banked $" + std::to_string(cash_) +
                                  " is safe. Carried loot lost.",
                           arenaW_ * 0.5f - 168.f, arenaH_ * 0.5f - 8.f, {220, 220, 230, 255});
                font_.draw(r, "Press R to dive again", arenaW_ * 0.5f - 96.f,
                           arenaH_ * 0.5f + 22.f, {180, 255, 190, 255});
            }
        }

        drawPortrait(r);  // emotes on top of everything, incl. the death screen
    }

    bool wantsQuit() const override { return quit_; }

    void shutdown() override {
        font_.close();
        bigFont_.close();
    }

private:
    struct Rect { float x, y, w, h; };

    // Derives the character's expression from live game state. Priority:
    // dead > just-banked > panic > scared > greedy > neutral.
    FaceState computeFace() const {
        if (gameOver_) return FaceState::Dead;
        if (bankFlash_ > 0.f) return FaceState::Happy;
        float nd = 1e9f;
        bool aggroNear = false;
        for (const auto& e : enemies_) {
            float d = (e.pos - player_).length();
            if (d < nd) nd = d;
            if (e.aggro && d < cfg::RoamerAggroRange) aggroNear = true;
        }
        float hp = health_ / cfg::PlayerMaxHealth;
        if (hp < 0.30f || (aggroNear && nd < 110.f)) return FaceState::Panic;
        if (aggroNear) return FaceState::Scared;
        if (carriedValue_ > 0) return FaceState::Greedy;
        return FaceState::Neutral;
    }

    void drawPortrait(Renderer& r) {
        const float pw = 104.f, ph = 104.f;
        const float px = arenaW_ - cfg::ArenaMargin - pw - 6.f;
        const float py = cfg::ArenaMargin + 6.f;
        FaceState s = computeFace();

        r.fillRect(px, py, pw, ph, {28, 28, 38, 255});
        Color border{90, 90, 110, 255};
        if (s == FaceState::Panic || s == FaceState::Dead)        border = {220, 70, 70, 255};
        else if (s == FaceState::Greedy || s == FaceState::Happy) border = {230, 200, 90, 255};
        else if (s == FaceState::Scared)                          border = {120, 160, 230, 255};
        r.outlineRect(px, py, pw, ph, border, 3);

        drawFace(r, s, px + 6.f, py + 2.f, pw - 12.f, ph - 22.f);
        if (font_.valid()) {
            std::string lbl = faceLabel(s);
            font_.draw(r, lbl, px + pw * 0.5f - lbl.size() * 4.5f, py + ph - 22.f, border);
        }
    }

    void renderGallery(Renderer& r) {
        r.clear({18, 18, 26, 255});
        const FaceState all[6] = {FaceState::Neutral, FaceState::Greedy, FaceState::Scared,
                                  FaceState::Panic, FaceState::Happy, FaceState::Dead};
        const float cellW = arenaW_ / 6.f;
        const float size = 118.f;
        for (int i = 0; i < 6; ++i) {
            float cx = cellW * i + cellW * 0.5f;
            float fy = arenaH_ * 0.5f - size * 0.5f - 10.f;
            drawFace(r, all[i], cx - size * 0.5f, fy, size, size);
            if (font_.valid()) {
                std::string lbl = faceLabel(all[i]);
                font_.draw(r, lbl, cx - lbl.size() * 4.5f, fy + size + 8.f, {200, 200, 220, 255});
            }
        }
        if (bigFont_.valid())
            bigFont_.draw(r, "HAUL  -  reactive faces", 30.f, 36.f, {230, 230, 240, 255});
    }

    // 0..1 pseudo-random, small LCG. Deterministic given the seed.
    float randf() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return ((rng_ >> 8) & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    void startRun() {
        player_ = {arenaW_ * 0.5f, arenaH_ * 0.5f};
        health_ = cfg::PlayerMaxHealth;
        carriedValue_ = 0;
        carriedWeight_ = 0.f;
        iframe_ = 0.f;
        danger_ = 0.f;
        nextThreshold_ = 0;
        bankFlash_ = 0.f;
        gameOver_ = false;

        loot_.clear();
        spawnLoot();
        enemies_.clear();
        for (int i = 0; i < cfg::RoamerCountBase; ++i) spawnRoamer();
    }

    void spawnLoot() {
        const int spanX = static_cast<int>(arenaW_ - 2 * cfg::ArenaMargin - 40);
        const int spanY = static_cast<int>(arenaH_ - 2 * cfg::ArenaMargin - 40);
        for (int i = 0; i < cfg::LootCount; ++i) {
            float x = cfg::ArenaMargin + 20.f + randf() * spanX;
            float y = cfg::ArenaMargin + 20.f + randf() * spanY;
            int ti = static_cast<int>(randf() * 3.f);
            if (ti > 2) ti = 2;
            const cfg::Tier& t = cfg::Tiers[ti];
            float s = 14.f + (t.value >= 80 ? 12.f : (t.value >= 30 ? 6.f : 0.f));
            loot_.push_back({{x, y}, {s, s}, t.value, t.weight, {t.r, t.g, t.b, 255}});
        }
    }

    void spawnRoamer() {
        Roamer e;
        const float lo = cfg::ArenaMargin + cfg::RoamerSize;
        for (int tries = 0; tries < 30; ++tries) {
            e.pos = {lo + randf() * (arenaW_ - 2 * lo), lo + randf() * (arenaH_ - 2 * lo)};
            if ((e.pos - player_).length() < 190.f) continue;          // not on the player
            if (e.pos.x >= zone_.x - 24 && e.pos.x <= zone_.x + zone_.w + 24 &&
                e.pos.y >= zone_.y - 24 && e.pos.y <= zone_.y + zone_.h + 24) continue;  // not in zone
            break;
        }
        float ang = randf() * 6.2831853f;
        e.heading = {std::cos(ang), std::sin(ang)};
        e.wanderTimer = 0.5f + randf() * 1.5f;
        enemies_.push_back(e);
    }

    void updateRoamer(Roamer& e, float dt) {
        Vec2 toPlayer = player_ - e.pos;
        float d = toPlayer.length();
        e.aggro = d < cfg::RoamerAggroRange;

        Vec2 dir;
        float spd;
        if (e.aggro) {
            dir = toPlayer.normalized();
            spd = cfg::RoamerChaseSpeed * (1.f + danger_ * cfg::DangerSpeedBonus);
            e.heading = dir;
        } else {
            e.wanderTimer -= dt;
            if (e.wanderTimer <= 0.f) {
                float ang = randf() * 6.2831853f;
                e.heading = {std::cos(ang), std::sin(ang)};
                e.wanderTimer = 1.0f + randf() * 2.0f;
            }
            dir = e.heading;
            spd = cfg::RoamerWanderSpeed;
        }
        e.pos += dir * (spd * dt);

        // Bounce off the arena bounds.
        const float b = cfg::ArenaMargin + cfg::RoamerSize * 0.5f;
        if (e.pos.x < b)            { e.pos.x = b;            e.heading.x = std::fabs(e.heading.x); }
        if (e.pos.x > arenaW_ - b)  { e.pos.x = arenaW_ - b;  e.heading.x = -std::fabs(e.heading.x); }
        if (e.pos.y < b)            { e.pos.y = b;            e.heading.y = std::fabs(e.heading.y); }
        if (e.pos.y > arenaH_ - b)  { e.pos.y = arenaH_ - b;  e.heading.y = -std::fabs(e.heading.y); }
    }

    Vec2 inputDir(const Input& in) const {
        Vec2 d{0.f, 0.f};
        if (in.down(SDL_SCANCODE_W) || in.down(SDL_SCANCODE_UP))    d.y -= 1.f;
        if (in.down(SDL_SCANCODE_S) || in.down(SDL_SCANCODE_DOWN))  d.y += 1.f;
        if (in.down(SDL_SCANCODE_A) || in.down(SDL_SCANCODE_LEFT))  d.x -= 1.f;
        if (in.down(SDL_SCANCODE_D) || in.down(SDL_SCANCODE_RIGHT)) d.x += 1.f;
        return d;
    }

    // Greedy-but-cowardly bot: flee a close enemy, else bank when loaded, else
    // grab the nearest loot. Enough to show the loop in a headless screenshot.
    Vec2 autopilotDir() {
        const Roamer* threat = nullptr;
        float td = 1e9f;
        for (const auto& e : enemies_) {
            float d = (e.pos - player_).length();
            if (d < td) { td = d; threat = &e; }
        }
        if (threat && td < 150.f) return player_ - threat->pos;  // panic

        Vec2 zoneCenter{zone_.x + zone_.w * 0.5f, zone_.y + zone_.h * 0.5f};
        if (carriedWeight_ >= cfg::MaxWeight * 0.6f) return zoneCenter - player_;

        const LootItem* best = nullptr;
        float bd = 1e9f;
        for (const auto& l : loot_) {
            if (l.collected) continue;
            float d = (l.pos - player_).length();
            if (d < bd) { bd = d; best = &l; }
        }
        if (best) return best->pos - player_;
        return zoneCenter - player_;
    }

    void drawBar(Renderer& r, float x, float y, float frac, Color fill, const std::string& label) {
        const float w = 200.f, h = 16.f;
        frac = cfg::clampf(frac, 0.f, 1.f);
        r.fillRect(x, y, w, h, {40, 40, 52, 255});
        r.fillRect(x, y, w * frac, h, fill);
        r.outlineRect(x, y, w, h, {90, 90, 110, 255}, 1);
        if (font_.valid()) font_.draw(r, label, x + w + 10.f, y - 2.f, {150, 150, 170, 255});
    }

    void drawHud(Renderer& r) {
        if (!font_.valid()) return;
        font_.draw(r, "CASH  $" + std::to_string(cash_), 34.f, 24.f, {255, 220, 120, 255});
        font_.draw(r, "CARRYING  $" + std::to_string(carriedValue_), 34.f, 48.f,
                   {200, 200, 220, 255});

        float hp = health_ / cfg::PlayerMaxHealth;
        drawBar(r, 34.f, 78.f, hp,
                {static_cast<unsigned char>(220 - 150 * hp),
                 static_cast<unsigned char>(70 + 150 * hp), 80, 255}, "HEALTH");

        float load = carriedWeight_ / cfg::MaxWeight;
        drawBar(r, 34.f, 102.f, load,
                {static_cast<unsigned char>(70 + 170 * cfg::clampf(load, 0.f, 1.f)),
                 static_cast<unsigned char>(200 - 150 * cfg::clampf(load, 0.f, 1.f)), 70, 255},
                "WEIGHT");

        drawBar(r, 34.f, 126.f, danger_,
                {static_cast<unsigned char>(200 + 55 * danger_),
                 static_cast<unsigned char>(120 - 100 * danger_), 40, 255}, "DANGER");

        if (bankFlash_ > 0.f && bigFont_.valid())
            bigFont_.draw(r, "BANKED!", zone_.x + 14.f, zone_.y - 36.f, {120, 255, 160, 255});
    }

    float arenaW_ = 0.f, arenaH_ = 0.f;
    Vec2 player_;
    Rect zone_{};
    std::vector<LootItem> loot_;
    std::vector<Roamer> enemies_;

    int cash_ = 0;
    int carriedValue_ = 0;
    float carriedWeight_ = 0.f;
    float health_ = cfg::PlayerMaxHealth;
    float iframe_ = 0.f;
    float danger_ = 0.f;
    int nextThreshold_ = 0;
    float bankFlash_ = 0.f;
    bool gameOver_ = false;

    unsigned rng_ = 1u;
    bool autoplay_ = false;
    bool gallery_ = false;
    bool quit_ = false;

    Font font_;
    Font bigFont_;
};

int main(int argc, char** argv) {
    AppConfig cfg;
    cfg.title = "HAUL (prototype)";
    cfg.width = 960;
    cfg.height = 600;

    bool demo = false;
    bool gallery = false;
    int frames = -1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--screenshot" && i + 1 < argc) {
            cfg.headless = true;
            cfg.screenshotPath = argv[++i];
        } else if (arg == "--demo") {
            demo = true;
        } else if (arg == "--faces") {
            gallery = true;
        } else if (arg == "--frames" && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        }
    }
    if (demo && frames < 0) frames = 240;
    if (frames > 0) cfg.warmupFrames = frames;

    HaulGame game;
    game.setAutoplay(demo);
    game.setGallery(gallery);
    App app(cfg);
    return app.run(game);
}
