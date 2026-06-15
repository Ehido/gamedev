#include "engine/Renderer.h"
#include <SDL_image.h>
#include <cmath>

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

void Renderer::drawLine(float x1, float y1, float x2, float y2, Color c, int thickness) {
    SDL_SetRenderDrawColor(sdl_, c.r, c.g, c.b, c.a);
    if (thickness <= 1) {
        SDL_RenderDrawLineF(sdl_, x1, y1, x2, y2);
        return;
    }
    // Thicken by drawing parallel lines offset along the perpendicular.
    float dx = x2 - x1, dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) {
        SDL_RenderDrawLineF(sdl_, x1, y1, x2, y2);
        return;
    }
    float nx = -dy / len, ny = dx / len;
    for (int i = 0; i < thickness; ++i) {
        float off = i - (thickness - 1) * 0.5f;
        SDL_RenderDrawLineF(sdl_, x1 + nx * off, y1 + ny * off, x2 + nx * off, y2 + ny * off);
    }
}

void Renderer::fillCircle(float cx, float cy, float radius, Color c) {
    SDL_SetRenderDrawColor(sdl_, c.r, c.g, c.b, c.a);
    int r0 = static_cast<int>(radius);
    for (int dy = -r0; dy <= r0; ++dy) {
        float dx = std::sqrt(radius * radius - static_cast<float>(dy) * dy);
        SDL_FRect row{cx - dx, cy + dy, 2 * dx, 1.f};
        SDL_RenderFillRectF(sdl_, &row);
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
