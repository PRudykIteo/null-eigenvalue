// The HUD's lettering.
//
// This app's chrome is one typographic idea: thin capitals, widely letterspaced,
// at a size just above the threshold of being ignorable. That needs exactly
// three things - a glyph atlas, a width measurement so a line can be centred,
// and per-character advance so the spacing can be opened up. Anything more is
// a text engine, and there is no paragraph in this app to justify one.
#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "palette.h"

namespace ne {

enum class Weight { Light, Regular };

// Atlases are rasterised per (weight, pixel size), so the lettering stays
// sharp when the window scale changes rather than being a scaled bitmap.
class TextRenderer {
 public:
    bool init(SDL_Renderer* r);
    void shutdown();

    // `tracking` is extra space between characters in pixels - the letterSpacing
    // the design is built on, which at these sizes carries more of the look than
    // the typeface does.
    float measure(const std::string& s, Weight w, float px, float tracking);

    void draw(const std::string& s, float x, float y, Weight w, float px,
              float tracking, Colour c);

    // Same, but x is the centre of the line rather than its left edge.
    void draw_centred(const std::string& s, float cx, float y, Weight w, float px,
                      float tracking, Colour c);

    // Distance from the baseline to the top of a capital, for vertical centring.
    float cap_height(Weight w, float px);

 private:
    struct Atlas;
    Atlas* atlas_for(Weight w, float px);

    SDL_Renderer* r_ = nullptr;
    // Small and fixed: the HUD uses four or five distinct sizes across two
    // weights, and a window resize changes them all at once.
    static constexpr int kMaxAtlases = 16;
    Atlas* atlases_[kMaxAtlases] = {};
    int atlas_count_ = 0;
};

}  // namespace ne
