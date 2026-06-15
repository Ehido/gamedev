#pragma once

namespace eng {

class Renderer;
class Input;

// Interface every game/scene implements. The engine drives these callbacks;
// the game never owns the loop. update() always receives dt in SECONDS and
// is called on a fixed timestep for deterministic simulation.
class Game {
public:
    virtual ~Game() = default;

    virtual void init(Renderer& /*r*/) {}
    virtual void update(float /*dt*/, const Input& /*in*/) {}
    virtual void render(Renderer& /*r*/) {}

    // Called by the engine after the loop ends but BEFORE SDL subsystems are
    // torn down. Games must release SDL-backed resources (fonts, textures)
    // here rather than in their destructor, which may run too late.
    virtual void shutdown() {}

    // Lets a game ask the engine to stop the loop (e.g. quit from a menu).
    virtual bool wantsQuit() const { return false; }
};

} // namespace eng
