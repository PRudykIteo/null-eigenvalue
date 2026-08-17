#include "palette.h"

namespace ne {
namespace {

// Index order is fixed and mirrors the engine's moods and the HUD dots.
const MoodPalette kPalettes[kPaletteCount] = {
    {"Kernel", Colour::rgb(0x05040B), Colour::rgb(0x1B1140), Colour::rgb(0x3E2C86),
     Colour::rgb(0x8A67F0)},
    {"Manifold", Colour::rgb(0x03070C), Colour::rgb(0x0E2A4A), Colour::rgb(0x1B7C97),
     Colour::rgb(0x5CE0CC)},
    {"Halo", Colour::rgb(0x05070B), Colour::rgb(0x14405C), Colour::rgb(0x52B6E4),
     Colour::rgb(0xFFE3AE)},
    {"Torsion", Colour::rgb(0x0A0406), Colour::rgb(0x4A0E32), Colour::rgb(0xB8235E),
     Colour::rgb(0xFF9152)},
    {"Limit", Colour::rgb(0x06070A), Colour::rgb(0x1C2733), Colour::rgb(0x4C6178),
     Colour::rgb(0xB6C9DA)},
    // Entropy is the noise mood, so it is the one palette with almost no hue
    // in it: warm grey over near-black, the colour of tape and room tone.
    {"Entropy", Colour::rgb(0x070707), Colour::rgb(0x262322), Colour::rgb(0x6B6560),
     Colour::rgb(0xE3DDD4)},
};

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

}  // namespace

// Component-wise in sRGB, not in linear light - the same space Flutter's
// Color.lerp used, so the crossfade looks like it always did.
Colour lerp(const Colour& a, const Colour& b, float t) {
    return Colour{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                  a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

const MoodPalette& palette_at(int mood) {
    if (mood < 0) mood = 0;
    if (mood >= kPaletteCount) mood = kPaletteCount - 1;
    return kPalettes[mood];
}

MoodPalette palette_lerp(const MoodPalette& a, const MoodPalette& b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return MoodPalette{t < 0.5f ? a.name : b.name, lerp(a.bg, b.bg, t),
                       lerp(a.deep, b.deep, t), lerp(a.mid, b.mid, t),
                       lerp(a.accent, b.accent, t)};
}

Colour MoodPalette::for_band(int i, int n, float centroid) const {
    const float t = n <= 1 ? 0.0f : (float)i / (float)(n - 1);
    const Colour base = t < 0.5f ? lerp(deep, mid, t * 2.0f)
                                 : lerp(mid, accent, (t - 0.5f) * 2.0f);
    // Brightness does not change hue, it lifts the whole thing toward accent -
    // which is what opening the filter actually sounds like.
    return lerp(base, accent, 0.28f * clamp01(centroid));
}

}  // namespace ne
