// HAUL (working title) -- a co-op scavenger game: grab loot, weight slows you
// down, haul it to the extraction zone, bank it for cash, push your luck.
//
// Milestone 1: the GREED LOOP. Walk over loot to pick it up (value + weight
// accumulate), the more you carry the slower you move, and standing on the
// extraction zone banks your carried value into cash. No danger yet -- that's
// next. Run with `--demo` to let a simple bot play it for a screenshot.

#include "engine/App.h"
#include "engine/Game.h"
#include "engine/Renderer.h"
#include "engine/Input.h"
#include "engine/Font.h"
#include "engine/Vec2.h"
#include "engine/Color.h"
#include "Config.h"

#include <SDL.h>
#include <string>
#include <vector>

using namespace eng;
namespace cfg = haul::cfg;

namespace {

struct LootItem {
    Vec2 pos;
    Vec2 size;
    int value;
    float weight;
    Color color;
    bool collected = false;
};

float dist(Vec2 a, Vec2 b) { return (a - b).length(); }

// Picks a font from bundled assets first, then common system locations.
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

    void init(Renderer& r) override {
        arenaW_ = static_cast<float>(r.width());
        arenaH_ = static_cast<float>(r.height());
        player_ = {arenaW_ * 0.5f, arenaH_ * 0.5f};

        zone_ = {cfg::ArenaMargin + 6.f, arenaH_ - cfg::ArenaMargin - 132.f, 152.f, 126.f};

        loadHudFont(font_, 18);
        loadHudFont(bigFont_, 26);

        // Deterministic scatter so screenshots are reproducible.
        unsigned seed = 1337u;
        auto rnd = [&seed]() {
            seed = seed * 1103515245u + 12345u;
            return (seed >> 16) & 0x7fff;
        };
        const int spanX = static_cast<int>(arenaW_ - 2 * cfg::ArenaMargin - 40);
        const int spanY = static_cast<int>(arenaH_ - 2 * cfg::ArenaMargin - 40);
        for (int i = 0; i < cfg::LootCount; ++i) {
            float x = cfg::ArenaMargin + 20 + (rnd() % spanX);
            float y = cfg::ArenaMargin + 20 + (rnd() % spanY);
            const cfg::Tier& t = cfg::Tiers[rnd() % 3];
            float s = 14.f + (t.value >= 80 ? 12.f : (t.value >= 30 ? 6.f : 0.f));
            loot_.push_back({{x, y}, {s, s}, t.value, t.weight, {t.r, t.g, t.b, 255}});
        }
    }

    void update(float dt, const Input& in) override {
        Vec2 dir = autoplay_ ? autopilotDir() : inputDir(in);

        if (dir.x != 0.f || dir.y != 0.f) {
            float speed = cfg::PlayerBaseSpeed * cfg::weightPenalty(carriedWeight_);
            player_ += dir.normalized() * (speed * dt);
        }

        const float lo = cfg::ArenaMargin + cfg::PlayerSize * 0.5f;
        if (player_.x < lo)           player_.x = lo;
        if (player_.x > arenaW_ - lo) player_.x = arenaW_ - lo;
        if (player_.y < lo)           player_.y = lo;
        if (player_.y > arenaH_ - lo) player_.y = arenaH_ - lo;

        // Pick up any loot within reach.
        for (auto& l : loot_) {
            if (l.collected) continue;
            if (dist(player_, l.pos) <= cfg::PickupRadius + l.size.x * 0.5f) {
                l.collected = true;
                carriedValue_ += l.value;
                carriedWeight_ += l.weight;
            }
        }

        // Bank carried loot when standing in the extraction zone.
        bool inZone = player_.x >= zone_.x && player_.x <= zone_.x + zone_.w &&
                      player_.y >= zone_.y && player_.y <= zone_.y + zone_.h;
        if (inZone && carriedValue_ > 0) {
            cash_ += carriedValue_;
            carriedValue_ = 0;
            carriedWeight_ = 0.f;
            bankFlash_ = 0.6f;
        }
        if (bankFlash_ > 0.f) bankFlash_ -= dt;

        if (in.down(SDL_SCANCODE_ESCAPE)) quit_ = true;
    }

    void render(Renderer& r) override {
        r.clear({18, 18, 26, 255});

        bool inZone = player_.x >= zone_.x && player_.x <= zone_.x + zone_.w &&
                      player_.y >= zone_.y && player_.y <= zone_.y + zone_.h;

        r.outlineRect(cfg::ArenaMargin, cfg::ArenaMargin,
                      arenaW_ - 2 * cfg::ArenaMargin, arenaH_ - 2 * cfg::ArenaMargin,
                      {64, 64, 86, 255}, 2);

        // Extraction zone, brighter when occupied or flashing from a deposit.
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

        // Player tints toward heavy/orange as the load grows.
        float load = carriedWeight_ / cfg::MaxWeight;
        Color pcol{static_cast<unsigned char>(220 + 35 * (load > 1.f ? 1.f : load)),
                   static_cast<unsigned char>(80 + 40 * (load > 1.f ? 1.f : load)),
                   70, 255};
        r.fillRectCentered(player_, {cfg::PlayerSize, cfg::PlayerSize}, pcol);
        r.outlineRect(player_.x - cfg::PlayerSize * 0.5f, player_.y - cfg::PlayerSize * 0.5f,
                      cfg::PlayerSize, cfg::PlayerSize, {255, 255, 255, 255}, 2);

        drawHud(r);
    }

    bool wantsQuit() const override { return quit_; }

    void shutdown() override {
        font_.close();
        bigFont_.close();
    }

private:
    struct Rect { float x, y, w, h; };

    Vec2 inputDir(const Input& in) const {
        Vec2 d{0.f, 0.f};
        if (in.down(SDL_SCANCODE_W) || in.down(SDL_SCANCODE_UP))    d.y -= 1.f;
        if (in.down(SDL_SCANCODE_S) || in.down(SDL_SCANCODE_DOWN))  d.y += 1.f;
        if (in.down(SDL_SCANCODE_A) || in.down(SDL_SCANCODE_LEFT))  d.x -= 1.f;
        if (in.down(SDL_SCANCODE_D) || in.down(SDL_SCANCODE_RIGHT)) d.x += 1.f;
        return d;
    }

    // Simple greedy bot: grab the nearest loot until fairly loaded, then go
    // bank it. Just enough to demonstrate the loop in a headless screenshot.
    Vec2 autopilotDir() {
        Vec2 zoneCenter{zone_.x + zone_.w * 0.5f, zone_.y + zone_.h * 0.5f};
        if (carriedWeight_ >= cfg::MaxWeight * 0.6f) {
            return zoneCenter - player_;
        }
        const LootItem* best = nullptr;
        float bestD = 1e9f;
        for (const auto& l : loot_) {
            if (l.collected) continue;
            float d = dist(player_, l.pos);
            if (d < bestD) { bestD = d; best = &l; }
        }
        if (best) return best->pos - player_;
        return zoneCenter - player_;  // nothing left: go bank
    }

    void drawHud(Renderer& r) {
        if (!font_.valid()) return;

        font_.draw(r, "CASH  $" + std::to_string(cash_), 34.f, 30.f, {255, 220, 120, 255});
        font_.draw(r, "CARRYING  $" + std::to_string(carriedValue_), 34.f, 54.f,
                   {200, 200, 220, 255});

        // Weight bar: green -> red as you approach/exceed capacity.
        float load = carriedWeight_ / cfg::MaxWeight;
        if (load > 1.f) load = 1.f;
        const float bx = 34.f, by = 84.f, bw = 220.f, bh = 16.f;
        r.fillRect(bx, by, bw, bh, {40, 40, 52, 255});
        unsigned char rr = static_cast<unsigned char>(70 + 170 * load);
        unsigned char gg = static_cast<unsigned char>(200 - 150 * load);
        r.fillRect(bx, by, bw * load, bh, {rr, gg, 70, 255});
        r.outlineRect(bx, by, bw, bh, {90, 90, 110, 255}, 1);
        font_.draw(r, "WEIGHT", bx + bw + 10.f, by - 2.f, {150, 150, 170, 255});

        if (bankFlash_ > 0.f && bigFont_.valid()) {
            bigFont_.draw(r, "BANKED!", zone_.x + 14.f, zone_.y - 34.f, {120, 255, 160, 255});
        }
    }

    float arenaW_ = 0.f, arenaH_ = 0.f;
    Vec2 player_;
    Rect zone_{};
    std::vector<LootItem> loot_;

    int cash_ = 0;
    int carriedValue_ = 0;
    float carriedWeight_ = 0.f;
    float bankFlash_ = 0.f;

    bool autoplay_ = false;
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
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--screenshot" && i + 1 < argc) {
            cfg.headless = true;
            cfg.screenshotPath = argv[++i];
        } else if (arg == "--demo") {
            demo = true;
        }
    }
    if (demo) cfg.warmupFrames = 240;  // ~4s of bot play before the snapshot

    HaulGame game;
    game.setAutoplay(demo);
    App app(cfg);
    return app.run(game);
}
