#include "engine/Input.h"

namespace eng {

void Input::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_QUIT) {
        quit_ = true;
    }
}

void Input::refreshKeyboard() {
    keys_ = SDL_GetKeyboardState(nullptr);
}

} // namespace eng
