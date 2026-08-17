// The colours of one mood.
//
// Four stops rather than a two-colour gradient because the picture is built
// out of additive blobs: `deep` is what the low register is drawn in and it
// has to stay almost invisible against `bg` when a voice is quiet, while
// `accent` is what a bell flashes in and has to survive being added on top of
// everything else. A single gradient between two colours cannot do both.
#pragma once

namespace ne {

struct Colour {
    float r = 0, g = 0, b = 0, a = 1;

    static constexpr Colour rgb(int hex) {
        return Colour{(float)((hex >> 16) & 0xFF) / 255.0f,
                      (float)((hex >> 8) & 0xFF) / 255.0f,
                      (float)(hex & 0xFF) / 255.0f, 1.0f};
    }
    constexpr Colour with_alpha(float alpha) const { return Colour{r, g, b, alpha}; }
};

Colour lerp(const Colour& a, const Colour& b, float t);

struct MoodPalette {
    const char* name;
    Colour bg;      // the page behind everything; never pure black
    Colour deep;    // the low register
    Colour mid;     // the middle, where most of the energy lives
    Colour accent;  // highs, bells and the pointer trail

    // The colour for register slice `i` of `n`, brightened by how bright the
    // engine says it currently sounds. Low slices sit near `deep` and the top
    // ones near `accent`, so the picture's vertical spread is the music's.
    Colour for_band(int i, int n, float centroid) const;
};

constexpr int kPaletteCount = 6;

const MoodPalette& palette_at(int mood);
MoodPalette palette_lerp(const MoodPalette& a, const MoodPalette& b, float t);

}  // namespace ne
