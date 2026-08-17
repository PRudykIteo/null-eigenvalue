// Draws the field.
//
// Every glowing thing on screen is the same white blob texture, tinted and
// added. The composition is one core (the drone), eight orbs (the register
// slices the engine publishes), rings for harmonic movement, points for bells,
// and the pointer's own wake - so what is on screen is not a decoration that
// happens to move, it is a reading of the synthesizer.
#pragma once

#include <SDL3/SDL.h>

#include "nebula.h"
#include "palette.h"

namespace ne {

class Renderer {
 public:
    bool init(SDL_Renderer* r);
    void shutdown();

    void draw(const NebulaState& state, int win_w, int win_h);

    // Chrome primitives. Blended rather than added: the HUD sits over a
    // picture that is sometimes very bright, and additive white on a saturated
    // orb is invisible exactly where the lettering matters most.
    void rect(float x, float y, float w, float h, Colour c);
    void tri(Vec2 a, Vec2 b, Vec2 c, Colour col);
    void disc(Vec2 centre, float radius, Colour c);
    void ring_at(Vec2 centre, float radius, float width, Colour c);

 private:
    void paint_field(const NebulaState& state, float w, float h);
    void paint_grain(float w, float h);

    void blob(Vec2 p, float radius, Colour c);
    void ring(Vec2 centre, float radius, float width, Colour c);
    void line(Vec2 a, Vec2 b, float width, Colour c);
    void triangle(Vec2 a, Vec2 b, Vec2 c, Colour col);

    SDL_Renderer* r_ = nullptr;
    SDL_Texture* blob_ = nullptr;
    SDL_Texture* grain_ = nullptr;

    // The reduced-resolution target the field is drawn into, kept between
    // frames and only rebuilt when its size changes.
    SDL_Texture* field_ = nullptr;
    int field_w_ = 0, field_h_ = 0;

    SDL_FRect bounds_{0, 0, 0, 0};
};

}  // namespace ne
