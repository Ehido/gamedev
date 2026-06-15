#pragma once
#include <string>
#include "engine/Renderer.h"
#include "engine/Input.h"

namespace eng {

class Game;

struct AppConfig {
    int width = 960;
    int height = 600;
    std::string title = "Engine";

    // Headless mode renders offscreen (software renderer) instead of opening
    // a window. Used on servers/CI and to capture screenshots.
    bool headless = false;
    std::string screenshotPath;  // if set in headless mode, save one frame here
    int warmupFrames = 1;        // update steps to run before the screenshot

    double fixedDt = 1.0 / 60.0; // simulation step, in seconds
};

// Owns platform setup (SDL), the window/renderer, and the main loop. The
// loop is a fixed-timestep accumulator: simulation advances in equal dt
// steps regardless of frame rate, which keeps physics stable and is the
// foundation needed for deterministic networked play later.
class App {
public:
    explicit App(AppConfig cfg);
    int run(Game& game);

private:
    AppConfig cfg_;
};

} // namespace eng
