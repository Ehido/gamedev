#pragma once
#include <SDL.h>

namespace eng {

// Snapshot of input state for a frame. Gameplay polls this instead of
// touching SDL directly, so the input source can change later.
class Input {
public:
    bool down(SDL_Scancode sc) const { return keys_ && keys_[sc]; }
    bool quitRequested() const { return quit_; }

    void handleEvent(const SDL_Event& e);
    void refreshKeyboard();

private:
    const Uint8* keys_ = nullptr;
    bool quit_ = false;
};

} // namespace eng
