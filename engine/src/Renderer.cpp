#include "engine/Renderer.h"
#include <SDL_image.h>

namespace eng {

Renderer::Renderer(SDL_Renderer* sdl, int width, int height)
    : sdl_(sdl), width_(width), height_(height) {}

void Renderer::clear(Color c) {
    SDL_SetRenderDrawColor(sdl_, c.r, c.g, c.b, c.a);
    SDL_RenderClear(sdl_);
}

void Renderer::fillRect(float x, float y, float w, float h, Color c) {
    SDL_SetRenderDrawColor(sdl_, c.r, c.g, c.b, c.a);
    SDL_FRect rect{x, y, w, h};
    SDL_RenderFillRectF(sdl_, &rect);
}

void Renderer::fillRectCentered(Vec2 center, Vec2 size, Color c) {
    fillRect(center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y, c);
}

void Renderer::outlineRect(float x, float y, float w, float h, Color c, int thickness) {
    SDL_SetRenderDrawColor(sdl_, c.r, c.g, c.b, c.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_FRect rect{x + i, y + i, w - 2 * i, h - 2 * i};
        SDL_RenderDrawRectF(sdl_, &rect);
    }
}

void Renderer::present() {
    SDL_RenderPresent(sdl_);
}

bool Renderer::savePNG(const std::string& path) {
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(
        0, width_, height_, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return false;

    if (SDL_RenderReadPixels(sdl_, nullptr, SDL_PIXELFORMAT_RGBA32,
                             surf->pixels, surf->pitch) != 0) {
        SDL_FreeSurface(surf);
        return false;
    }

    int rc = IMG_SavePNG(surf, path.c_str());
    SDL_FreeSurface(surf);
    return rc == 0;
}

} // namespace eng
