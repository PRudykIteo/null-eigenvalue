// The two images the whole picture is drawn out of.
//
// Everything glowing in this app is the same white blob, tinted and scaled.
// The alternative - building a radial gradient per orb per frame - works, but
// it is fifteen gradient objects a frame where one texture drawn fifteen times
// is a single GPU state change.
//
// Both are generated rather than shipped: it keeps the palette a one-line edit
// and there is no asset to get out of step with the code.
#pragma once

#include <cstdint>
#include <vector>

namespace ne {

struct TextureData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;  // tightly packed, width * height * 4
};

// A soft white disc, alpha falling off as a raised cosine squared. The
// exponent is the whole character of the thing: too low and the orbs have
// visible edges, too high and they are a faint smudge with no core.
TextureData make_blob(int size);

// Monochrome noise, tiled over the finished frame. Without it, wide dark
// gradients on an 8-bit display band into visible rings; with it, the dither
// hides them and the picture reads as film rather than as a gradient.
//
// One to three levels, added rather than overlaid. Below one level there is
// nothing to break a contour with; much above three and the lift of the blacks
// is visible as haze on a picture that is mostly black.
TextureData make_grain(int size);

}  // namespace ne
