// Everything the picture needs that is not in the engine: orbit phases, the
// spring that chases the pointer, and the three kinds of event the music
// throws off.
//
// Kept apart from the renderer so the simulation runs once per frame at a
// known dt, rather than inside the draw call where it would run again for
// every repaint.
#pragma once

#include <vector>

// For Rng and kTwoPi. The primitives are header-only PODs that allocate
// nothing, so the picture borrowing the engine's random generator costs a
// header rather than a dependency - and means the grain and the spark
// placement are identical on every platform.
#include "dsp.h"
#include "nulleig.h"
#include "palette.h"

namespace ne {

struct Vec2 {
    float x = 0, y = 0;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return Vec2{a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return Vec2{a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return Vec2{a.x * s, a.y * s}; }

struct TrailPoint {
    Vec2 p;
    float age = 0;
};

struct Ripple {
    float age = 0;
};

struct Spark {
    Vec2 pos;
    Vec2 drift;
    float size = 1;
    float age = 0;
};

class NebulaState {
 public:
    NebulaState();

    void advance(float dt, const ne_vis& v);
    void add_trail(Vec2 normalised);

    // The orbit offset for band `i` at the current time, before the vertical
    // stretch the renderer applies.
    Vec2 orbit(int i, float radius) const;

    float t = 0;

    // Where the picture thinks the field is. A critically damped spring, so a
    // flick has weight and a slow drag tracks exactly.
    Vec2 centre{0.5f, 0.55f};
    Vec2 target{0.5f, 0.55f};

    ne_vis vis{};
    MoodPalette palette = palette_at(1);

    // 0 while playing, 1 while stopped: fades in the invitation to start
    // without putting a permanent button on the screen.
    float idle_hint = 1;

    // The fraction of the window's real pixels the field is rasterised at,
    // before being scaled back up. 1 draws at full resolution.
    float render_scale = 1;

    const std::vector<TrailPoint>& trail() const { return trail_; }
    const std::vector<Ripple>& ripples() const { return ripples_; }
    const std::vector<Spark>& sparks() const { return sparks_; }

 private:
    // Golden-ratio phases and speeds, the same trick the synthesizer uses on
    // its breath periods and for the same reason: no two orbits share a period
    // so the arrangement never visibly repeats.
    float wa_[NE_BANDS], wb_[NE_BANDS], wc_[NE_BANDS];
    float pa_[NE_BANDS], pb_[NE_BANDS], pc_[NE_BANDS];

    Vec2 vel_{0, 0};

    std::vector<TrailPoint> trail_;
    std::vector<Ripple> ripples_;
    std::vector<Spark> sparks_;

    int last_chord_ = -1;
    float last_spark_ = 0;
    Rng rnd_;
};

}  // namespace ne
