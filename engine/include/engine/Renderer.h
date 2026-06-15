#pragma once
#include <SDL.h>
#include <string>
#include "engine/Color.h"
#include "engine/Vec2.h"

namespace eng {

// Thin drawing abstraction over an SDL_Renderer. The engine owns the SDL
// handle; the game only ever talks to this interface, so we can swap the
// backend later (e.g. a custom GPU path) without touching gameplay code.
class Renderer {
public:
    Renderer(SDL_Renderer* sdl, int width, int height);

    void clear(Color c);
    void fillRect(float x, float y, float w, float h, Color c);
    void fillRectCentered(Vec2 center, Vec2 size, Color c);
    void outlineRect(float x, float y, float w, float h, Color c, int thickness = 2);
    void present();

    // Reads the current framebuffer back and writes it to a PNG. Works for
    // both windowed and headless (software) renderers, which is how the
    // engine produces screenshots without a display.
    bool savePNG(const std::string& path);

    int width() const { return width_; }
    int height() const { return height_; }
    SDL_Renderer* sdl() const { return sdl_; }

private:
    SDL_Renderer* sdl_;
    int width_;
    int height_;
};

} // namespace eng
