#include "nebula.h"

#include <algorithm>
#include <cmath>

namespace ne {
namespace {

constexpr int kMaxTrail = 90;
constexpr int kMaxRipples = 8;
constexpr int kMaxSparks = 24;

constexpr float kTrailLife = 1.1f;
constexpr float kRippleLife = 7.0f;
constexpr float kSparkLife = 3.2f;

inline float fract(float x) { return x - std::floor(x); }

}  // namespace

NebulaState::NebulaState() {
    for (int i = 0; i < NE_BANDS; ++i) {
        const float g = fract((float)i * 0.6180339887f);
        const float h = fract((float)i * 0.3819660113f + 0.37f);
        wa_[i] = 0.013f + 0.030f * g;
        wb_[i] = 0.011f + 0.026f * h;
        wc_[i] = 0.006f + 0.013f * fract((float)i * 0.7548776662f);
        pa_[i] = g * kTwoPi;
        pb_[i] = h * kTwoPi;
        pc_[i] = fract((float)i * 0.5436890127f) * kTwoPi;
    }
    rnd_.seed(0xE16Eu, 1);
    trail_.reserve(kMaxTrail);
    ripples_.reserve(kMaxRipples);
    sparks_.reserve(kMaxSparks);
}

void NebulaState::advance(float dt, const ne_vis& v) {
    t += dt;
    vis = v;

    // omega picked so that a full-screen move settles in about a third of a
    // second - fast enough to feel connected, slow enough to have mass.
    constexpr float omega = 11.0f;
    const Vec2 dx = target - centre;
    vel_ = vel_ + (dx * (omega * omega) - vel_ * (2.0f * omega)) * dt;
    centre = centre + vel_ * dt;

    for (auto& p : trail_) p.age += dt;
    trail_.erase(std::remove_if(trail_.begin(), trail_.end(),
                                [](const TrailPoint& p) { return p.age > kTrailLife; }),
                 trail_.end());

    for (auto& r : ripples_) r.age += dt;
    ripples_.erase(std::remove_if(ripples_.begin(), ripples_.end(),
                                  [](const Ripple& r) { return r.age > kRippleLife; }),
                   ripples_.end());

    for (auto& s : sparks_) {
        s.age += dt;
        s.pos = s.pos + s.drift * dt;
    }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(),
                                 [](const Spark& s) { return s.age > kSparkLife; }),
                  sparks_.end());

    // The harmony moving is worth seeing: one slow ring per voice entry.
    if (last_chord_ < 0) last_chord_ = v.chord_change;
    if (v.chord_change != last_chord_) {
        last_chord_ = v.chord_change;
        if ((int)ripples_.size() < kMaxRipples) ripples_.push_back(Ripple{});
    }

    // A bell is a rising edge on `spark`, not a level.
    if (v.spark > last_spark_ + 0.25f && (int)sparks_.size() < kMaxSparks) {
        const float a = rnd_.uni() * kTwoPi;
        const float r = 0.10f + 0.34f * rnd_.uni();
        const Vec2 dir{std::cos(a), std::sin(a)};
        sparks_.push_back(Spark{centre + dir * r,
                                dir * 0.012f - Vec2{0.0f, 0.008f},
                                0.6f + 0.8f * rnd_.uni(), 0.0f});
    }
    last_spark_ = v.spark;
}

void NebulaState::add_trail(Vec2 normalised) {
    trail_.push_back(TrailPoint{normalised, 0.0f});
    if ((int)trail_.size() > kMaxTrail) trail_.erase(trail_.begin());
}

Vec2 NebulaState::orbit(int i, float radius) const {
    const float rr = radius * (1.0f + 0.20f * std::sin(t * wc_[i] * kTwoPi + pc_[i]));
    return Vec2{std::cos(t * wa_[i] * kTwoPi + pa_[i]),
                std::sin(t * wb_[i] * kTwoPi + pb_[i])} * rr;
}

}  // namespace ne
