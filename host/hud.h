// The chrome: a wordmark at the top and one centred stack at the bottom.
//
// It is deliberately the same four lines it has always been - transport, the
// six moods, what is playing, and its name - because the picture is the app
// and everything here is a footnote to it. The panel behind the gear is where
// anything that needs explaining lives.
#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "palette.h"
#include "render.h"
#include "text.h"

namespace ne {

struct HudModel {
    int mood = 1;
    bool playing = false;
    float root_hz = 55.0f;
    std::string token;
    std::string version;   // empty in a build CI did not cut
    const char* mood_name = "Manifold";

    // 0 hidden, 1 fully raised. The chrome crossfades rather than appearing.
    float amount = 0;
};

// Where a click landed, so the caller can act without the HUD knowing what a
// controller is.
enum class HudHit { None, Transport, Mood, Token };

struct HudResult {
    HudHit hit = HudHit::None;
    int mood = -1;   // set when hit == Mood
};

class Hud {
 public:
    void layout(const HudModel& m, float w, float h, float scale, TextRenderer& text);
    void draw(const HudModel& m, const MoodPalette& p, Renderer& r, TextRenderer& text);

    // Hit testing runs against the last layout, so it is only meaningful while
    // the chrome is actually up.
    HudResult hit_test(float x, float y) const;

 private:
    float w_ = 0, h_ = 0, scale_ = 1;
    SDL_FRect transport_{0, 0, 0, 0};
    SDL_FRect dots_[kPaletteCount] = {};
    SDL_FRect token_{0, 0, 0, 0};

    float dots_y_ = 0;
    float name_y_ = 0;
    float hz_y_ = 0;
    float token_y_ = 0;
    float wordmark_y_ = 0;
};

}  // namespace ne
