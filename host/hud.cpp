#include "hud.h"

#include <cmath>
#include <cstdio>

namespace ne {
namespace {

// The dot row's geometry. Small, evenly spaced, and far enough apart that a
// mouse can pick one without aiming.
constexpr float kDotGap = 22.0f;
constexpr float kDotR = 3.0f;

// How far in. A piece has no length, so this is not a position in something -
// it is how long you have been listening, and the only number that makes
// "half an hour in" a thing you can write down.
std::string elapsed_label(double seconds) {
    const int t = (int)seconds;
    char buf[32];
    if (t >= 3600) {
        std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
    } else {
        std::snprintf(buf, sizeof(buf), "%d:%02d", t / 60, t % 60);
    }
    return std::string(buf);
}

std::string hz_label(float hz) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f Hz", (double)hz);
    return std::string(buf);
}

// The engine names its moods "Kernel", "Halo"; the chrome is set in capitals
// throughout, and at 4.6 px of tracking a lowercase letter reads as a mistake.
std::string caps(const char* s) {
    std::string out(s ? s : "");
    for (char& c : out) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    }
    return out;
}

}  // namespace

void Hud::layout(const HudModel& m, float w, float h, float scale, TextRenderer& text) {
    (void)m;
    (void)text;
    w_ = w;
    h_ = h;
    scale_ = scale;

    // Built from the bottom up, because the bottom margin is what the eye
    // measures this stack against.
    const float bottom = h - 34.0f * scale;

    token_y_ = bottom - 10.0f * scale;
    hz_y_ = token_y_ - 14.0f * scale;
    name_y_ = hz_y_ - 20.0f * scale;
    dots_y_ = name_y_ - 22.0f * scale;

    const float span = kDotGap * scale * (float)(kPaletteCount - 1);
    const float x0 = w * 0.5f - span * 0.5f;
    for (int i = 0; i < kPaletteCount; ++i) {
        const float cx = x0 + kDotGap * scale * (float)i;
        // The hit rect is deliberately far bigger than the dot.
        const float r = 11.0f * scale;
        dots_[i] = SDL_FRect{cx - r, dots_y_ - r, r * 2.0f, r * 2.0f};
    }

    const float tr = 15.0f * scale;
    transport_ = SDL_FRect{w * 0.5f - tr, dots_y_ - 30.0f * scale - tr, tr * 2.0f,
                           tr * 2.0f};

    token_ = SDL_FRect{w * 0.5f - 70.0f * scale, token_y_ - 11.0f * scale,
                       140.0f * scale, 20.0f * scale};

    wordmark_y_ = 22.0f * scale;
}

void Hud::draw(const HudModel& m, const MoodPalette& p, Renderer& r, TextRenderer& text) {
    const float a = m.amount;
    if (a <= 0.01f) return;

    // ---- wordmark ---------------------------------------------------------
    // The name, and after it the version at half its weight. A downloaded app
    // has no store page to look at, so "which one am I running" has to be
    // answerable from the app itself - but it is a footnote to the title, not
    // a second title, so it shares the line.
    {
        const float px = 11.0f * scale_;
        const float track = 6.5f * scale_;
        const float vpx = 10.0f * scale_;
        const float vtrack = 2.4f * scale_;
        const std::string ver = m.version.empty() ? "  DEV" : ("  " + m.version);
        const float total =
            text.measure("NULL EIGENVALUE", Weight::Light, px, track) +
            text.measure(ver, Weight::Light, vpx, vtrack);
        float x = w_ * 0.5f - total * 0.5f;
        const float y = wordmark_y_ + text.cap_height(Weight::Light, px);
        text.draw("NULL EIGENVALUE", x, y, Weight::Light, px, track,
                  Colour{1, 1, 1, 0.34f * a});
        x += text.measure("NULL EIGENVALUE", Weight::Light, px, track);
        text.draw(ver, x, y, Weight::Light, vpx, vtrack, Colour{1, 1, 1, 0.18f * a});
    }

    // ---- transport --------------------------------------------------------
    // Drawn rather than iconified: a Material glyph in this picture looks like
    // a sticker on it.
    {
        const float cx = transport_.x + transport_.w * 0.5f;
        const float cy = transport_.y + transport_.h * 0.5f;
        const float s = 9.0f * scale_;
        const Colour c = p.accent.with_alpha(0.85f * a);
        if (m.playing) {
            const float bw = s * 0.30f;
            r.rect(cx - s * 0.45f - bw * 0.5f, cy - s, bw, s * 2.0f, c);
            r.rect(cx + s * 0.45f - bw * 0.5f, cy - s, bw, s * 2.0f, c);
        } else {
            r.tri(Vec2{cx - s * 0.45f, cy - s}, Vec2{cx - s * 0.45f, cy + s},
                  Vec2{cx + s * 0.95f, cy}, c);
        }
    }

    // ---- the six moods ----------------------------------------------------
    for (int i = 0; i < kPaletteCount; ++i) {
        const float cx = dots_[i].x + dots_[i].w * 0.5f;
        const bool on = i == m.mood;
        // The current one is a filled dot, the rest are rings: the row reads
        // as a position rather than as six buttons.
        if (on) {
            r.disc(Vec2{cx, dots_y_}, kDotR * scale_ * 1.35f,
                   p.accent.with_alpha(0.95f * a));
        } else {
            r.ring_at(Vec2{cx, dots_y_}, kDotR * scale_, 1.0f * scale_,
                      p.accent.with_alpha(0.34f * a));
        }
    }

    // ---- what is playing --------------------------------------------------
    text.draw_centred(caps(m.instrument.c_str()), w_ * 0.5f, name_y_, Weight::Regular,
                      12.0f * scale_, 4.6f * scale_, p.accent.with_alpha(0.92f * a));

    // The frequency and the clock share a line: both are readings of what is
    // true now, and neither is worth a row of its own.
    text.draw_centred(hz_label(m.root_hz) + "   " + elapsed_label(m.elapsed),
                      w_ * 0.5f, hz_y_, Weight::Light, 10.0f * scale_,
                      2.4f * scale_, Colour{1, 1, 1, 0.26f * a});

    // The name of what is playing, directly under the frequency because the
    // two are the same kind of thing - a reading of what is currently true.
    if (!m.token.empty()) {
        text.draw_centred(m.token, w_ * 0.5f, token_y_, Weight::Regular,
                          10.0f * scale_, 1.6f * scale_, Colour{1, 1, 1, 0.40f * a});
    }
}

HudResult Hud::hit_test(float x, float y) const {
    auto in = [&](const SDL_FRect& r) {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    };
    if (in(transport_)) return HudResult{HudHit::Transport, -1};
    for (int i = 0; i < kPaletteCount; ++i) {
        if (in(dots_[i])) return HudResult{HudHit::Mood, i};
    }
    if (in(token_)) return HudResult{HudHit::Token, -1};
    return HudResult{HudHit::None, -1};
}

}  // namespace ne
