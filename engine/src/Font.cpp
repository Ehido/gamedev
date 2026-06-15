#include "engine/Font.h"
#include "engine/Renderer.h"

namespace eng {

Font::~Font() {
    close();
}

void Font::close() {
    if (font_) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
}

bool Font::load(const std::string& path, int ptSize) {
    if (font_) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
    font_ = TTF_OpenFont(path.c_str(), ptSize);
    return font_ != nullptr;
}

int Font::lineHeight() const {
    return font_ ? TTF_FontHeight(font_) : 0;
}

void Font::draw(Renderer& r, const std::string& text, float x, float y, Color c) {
    if (!font_ || text.empty()) return;

    SDL_Color col{c.r, c.g, c.b, c.a};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, text.c_str(), col);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(r.sdl(), surf);
    if (tex) {
        SDL_FRect dst{x, y, static_cast<float>(surf->w), static_cast<float>(surf->h)};
        SDL_RenderCopyF(r.sdl(), tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

} // namespace eng
