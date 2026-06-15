#include "engine/App.h"
#include "engine/Game.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <cstdlib>
#include <utility>

namespace eng {

App::App(AppConfig cfg) : cfg_(std::move(cfg)) {}

int App::run(Game& game) {
    if (cfg_.headless) {
        // No display/audio device in headless environments (servers, CI).
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        setenv("SDL_AUDIODRIVER", "dummy", 1);
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    SDL_Window* window = nullptr;
    SDL_Surface* surface = nullptr;
    SDL_Renderer* sdlr = nullptr;

    if (cfg_.headless) {
        surface = SDL_CreateRGBSurfaceWithFormat(
            0, cfg_.width, cfg_.height, 32, SDL_PIXELFORMAT_RGBA32);
        if (!surface) {
            SDL_Log("surface create failed: %s", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        sdlr = SDL_CreateSoftwareRenderer(surface);
    } else {
        window = SDL_CreateWindow(cfg_.title.c_str(),
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  cfg_.width, cfg_.height, SDL_WINDOW_SHOWN);
        if (!window) {
            SDL_Log("window create failed: %s", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        sdlr = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!sdlr) {
            sdlr = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }
    }

    if (!sdlr) {
        SDL_Log("renderer create failed: %s", SDL_GetError());
        if (surface) SDL_FreeSurface(surface);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(sdlr, SDL_BLENDMODE_BLEND);

    Renderer renderer(sdlr, cfg_.width, cfg_.height);
    game.init(renderer);

    int rc = 0;

    if (cfg_.headless) {
        // Advance the sim a few steps, render one frame, snapshot it.
        Input emptyInput;
        for (int i = 0; i < cfg_.warmupFrames; ++i) {
            game.update(static_cast<float>(cfg_.fixedDt), emptyInput);
        }
        game.render(renderer);
        if (!cfg_.screenshotPath.empty()) {
            if (!renderer.savePNG(cfg_.screenshotPath)) {
                SDL_Log("savePNG failed: %s", SDL_GetError());
                rc = 2;
            }
        }
    } else {
        Input input;
        Uint64 prev = SDL_GetPerformanceCounter();
        const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
        double accumulator = 0.0;
        bool running = true;

        while (running) {
            Uint64 now = SDL_GetPerformanceCounter();
            double frame = static_cast<double>(now - prev) / freq;
            prev = now;
            if (frame > 0.25) frame = 0.25;  // avoid spiral-of-death after a stall
            accumulator += frame;

            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = false;
                input.handleEvent(e);
            }
            input.refreshKeyboard();

            while (accumulator >= cfg_.fixedDt) {
                game.update(static_cast<float>(cfg_.fixedDt), input);
                accumulator -= cfg_.fixedDt;
            }

            if (game.wantsQuit() || input.quitRequested()) running = false;

            game.render(renderer);
            renderer.present();
        }
    }

    // Let the game release SDL-backed resources while subsystems are alive.
    game.shutdown();

    SDL_DestroyRenderer(sdlr);
    if (surface) SDL_FreeSurface(surface);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return rc;
}

} // namespace eng
