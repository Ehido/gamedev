#pragma once
#include <SDL_ttf.h>
#include <string>
#include "engine/Color.h"

namespace eng {

class Renderer;

// TrueType text rendering. Loads a .ttf at a fixed point size and draws
// strings through the engine Renderer. Non-copyable: it owns an SDL handle.
class Font {
public:
    Font() = default;
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    bool load(const std::string& path, int ptSize);
    bool valid() const { return font_ != nullptr; }

    // Releases the underlying TTF handle. Call before SDL_ttf is shut down.
    void close();

    // Draws text with its top-left corner at (x, y).
    void draw(Renderer& r, const std::string& text, float x, float y, Color c);

    int lineHeight() const;

private:
    TTF_Font* font_ = nullptr;
};

} // namespace eng
