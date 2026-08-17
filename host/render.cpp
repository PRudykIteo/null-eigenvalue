#include "render.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "textures.h"

namespace ne {
namespace {

inline float clampf01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Cubic ease-out, the curve the ripples expand on.
inline float ease_out_cubic(float t) {
    const float u = 1.0f - clampf01(t);
    return 1.0f - u * u * u;
}

SDL_Texture* upload(SDL_Renderer* r, const TextureData& t, SDL_ScaleMode mode) {
    SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, t.width, t.height);
    if (!tex) return nullptr;
    SDL_UpdateTexture(tex, nullptr, t.rgba.data(), t.width * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
    SDL_SetTextureScaleMode(tex, mode);
    return tex;
}

}  // namespace

bool Renderer::init(SDL_Renderer* r) {
    r_ = r;
    blob_ = upload(r, make_blob(256), SDL_SCALEMODE_LINEAR);
    // Nearest, because the grain is the one thing here that is meant to be
    // per-pixel; filtering it would average the dither away.
    grain_ = upload(r, make_grain(512), SDL_SCALEMODE_NEAREST);
    return blob_ != nullptr && grain_ != nullptr;
}

void Renderer::shutdown() {
    if (blob_) SDL_DestroyTexture(blob_);
    if (grain_) SDL_DestroyTexture(grain_);
    if (field_) SDL_DestroyTexture(field_);
    blob_ = grain_ = field_ = nullptr;
}

void Renderer::blob(Vec2 p, float radius, Colour c) {
    if (radius <= 0.2f || c.a <= 0.002f) return;

    const SDL_FRect dst{p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f};

    // Trim the quad to what is actually on screen, and take the matching
    // corner out of the source rectangle so the picture is unchanged.
    //
    // The blobs are deliberately far larger than the window: the pool of light
    // under the field has a radius of 1.15 times the geometric mean of the two
    // sides, which on a large window is a quad several times the area of the
    // screen it is drawn on. The backend does clip it, but not before the
    // whole thing has been through the blend, and there are fifteen a frame.
    const float l = std::max(dst.x, bounds_.x);
    const float t = std::max(dst.y, bounds_.y);
    const float rr = std::min(dst.x + dst.w, bounds_.x + bounds_.w);
    const float bb = std::min(dst.y + dst.h, bounds_.y + bounds_.h);
    if (rr <= l || bb <= t) return;

    const SDL_FRect vis{l, t, rr - l, bb - t};
    const float tw = 256.0f, th = 256.0f;
    const SDL_FRect src{(vis.x - dst.x) / dst.w * tw, (vis.y - dst.y) / dst.h * th,
                        vis.w / dst.w * tw, vis.h / dst.h * th};

    SDL_SetTextureColorModFloat(blob_, c.r, c.g, c.b);
    SDL_SetTextureAlphaModFloat(blob_, c.a);
    SDL_RenderTexture(r_, blob_, &src, &vis);
}

void Renderer::ring(Vec2 centre, float radius, float width, Colour c) {
    if (radius <= 0.5f || c.a <= 0.002f) return;
    // Segment count follows the radius: a big ring drawn with thirty segments
    // is a visible polygon, a small one drawn with two hundred is waste.
    const int seg = std::min(256, std::max(24, (int)(radius * 0.5f)));
    const float hw = width * 0.5f;

    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    verts.reserve((size_t)(seg + 1) * 2);
    idx.reserve((size_t)seg * 6);

    const SDL_FColor col{c.r, c.g, c.b, c.a};
    for (int i = 0; i <= seg; ++i) {
        const float a = kTwoPi * (float)i / (float)seg;
        const float cx = std::cos(a), sy = std::sin(a);
        verts.push_back(SDL_Vertex{
            {centre.x + cx * (radius - hw), centre.y + sy * (radius - hw)}, col, {0, 0}});
        verts.push_back(SDL_Vertex{
            {centre.x + cx * (radius + hw), centre.y + sy * (radius + hw)}, col, {0, 0}});
    }
    for (int i = 0; i < seg; ++i) {
        const int a = i * 2, b = i * 2 + 1, c2 = i * 2 + 2, d = i * 2 + 3;
        idx.insert(idx.end(), {a, b, c2, b, d, c2});
    }
    SDL_RenderGeometry(r_, nullptr, verts.data(), (int)verts.size(), idx.data(),
                       (int)idx.size());
}

void Renderer::line(Vec2 a, Vec2 b, float width, Colour c) {
    if (c.a <= 0.002f) return;
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    const float nx = -dy / len * width * 0.5f;
    const float ny = dx / len * width * 0.5f;

    const SDL_FColor col{c.r, c.g, c.b, c.a};
    SDL_Vertex v[4] = {
        {{a.x + nx, a.y + ny}, col, {0, 0}},
        {{a.x - nx, a.y - ny}, col, {0, 0}},
        {{b.x + nx, b.y + ny}, col, {0, 0}},
        {{b.x - nx, b.y - ny}, col, {0, 0}},
    };
    int idx[6] = {0, 1, 2, 1, 3, 2};
    SDL_RenderGeometry(r_, nullptr, v, 4, idx, 6);
}

void Renderer::triangle(Vec2 a, Vec2 b, Vec2 c, Colour col) {
    const SDL_FColor fc{col.r, col.g, col.b, col.a};
    SDL_Vertex v[3] = {
        {{a.x, a.y}, fc, {0, 0}},
        {{b.x, b.y}, fc, {0, 0}},
        {{c.x, c.y}, fc, {0, 0}},
    };
    SDL_RenderGeometry(r_, nullptr, v, 3, nullptr, 0);
}

void Renderer::rect(float x, float y, float w, float h, Colour c) {
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColorFloat(r_, c.r, c.g, c.b, c.a);
    const SDL_FRect f{x, y, w, h};
    SDL_RenderFillRect(r_, &f);
}

void Renderer::tri(Vec2 a, Vec2 b, Vec2 c, Colour col) {
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    triangle(a, b, c, col);
}

void Renderer::disc(Vec2 centre, float radius, Colour c) {
    if (radius <= 0.2f || c.a <= 0.002f) return;
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    const int seg = std::min(64, std::max(12, (int)(radius * 4.0f)));
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    const SDL_FColor col{c.r, c.g, c.b, c.a};
    verts.push_back(SDL_Vertex{{centre.x, centre.y}, col, {0, 0}});
    for (int i = 0; i <= seg; ++i) {
        const float ang = kTwoPi * (float)i / (float)seg;
        verts.push_back(SDL_Vertex{
            {centre.x + std::cos(ang) * radius, centre.y + std::sin(ang) * radius},
            col,
            {0, 0}});
    }
    for (int i = 1; i <= seg; ++i) idx.insert(idx.end(), {0, i, i + 1});
    SDL_RenderGeometry(r_, nullptr, verts.data(), (int)verts.size(), idx.data(),
                       (int)idx.size());
}

void Renderer::ring_at(Vec2 centre, float radius, float width, Colour c) {
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    ring(centre, radius, width, c);
}

void Renderer::draw(const NebulaState& state, int win_w, int win_h) {
    const float w = (float)win_w, h = (float)win_h;
    const float scale = state.render_scale;
    const int fw = (int)std::lround(w * scale);
    const int fh = (int)std::lround(h * scale);

    if (scale >= 0.999f || fw < 16 || fh < 16) {
        SDL_SetRenderTarget(r_, nullptr);
        paint_field(state, w, h);
        paint_grain(w, h);
        return;
    }

    // Draw the field into a smaller texture and stretch it back over the
    // window. The whole composition is soft blobs with no edge anywhere in it,
    // so there is no detail to lose - and the cost of this picture is almost
    // entirely blended pixels, which this scales by the square. The grain goes
    // on afterwards at full resolution: it is the one thing here that is meant
    // to be per-pixel, and it doubles as dither for the upscale.
    if (!field_ || field_w_ != fw || field_h_ != fh) {
        if (field_) SDL_DestroyTexture(field_);
        field_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32,
                                   SDL_TEXTUREACCESS_TARGET, fw, fh);
        SDL_SetTextureScaleMode(field_, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(field_, SDL_BLENDMODE_NONE);
        field_w_ = fw;
        field_h_ = fh;
    }

    SDL_SetRenderTarget(r_, field_);
    // The field is drawn in window coordinates and scaled on the way in, so
    // every radius in paint_field stays in the units it was designed in.
    SDL_SetRenderScale(r_, (float)fw / w, (float)fh / h);
    paint_field(state, w, h);
    SDL_SetRenderScale(r_, 1.0f, 1.0f);

    SDL_SetRenderTarget(r_, nullptr);
    const SDL_FRect dst{0, 0, w, h};
    SDL_RenderTexture(r_, field_, nullptr, &dst);
    paint_grain(w, h);
}

void Renderer::paint_grain(float w, float h) {
    // Tiled at one texel per pixel: the noise is dither, so scaling it would
    // defeat the point.
    for (float y = 0; y < h; y += 512.0f) {
        for (float x = 0; x < w; x += 512.0f) {
            const SDL_FRect dst{x, y, 512.0f, 512.0f};
            SDL_RenderTexture(r_, grain_, nullptr, &dst);
        }
    }
}

void Renderer::paint_field(const NebulaState& state, float w, float h) {
    bounds_ = SDL_FRect{0, 0, w, h};

    // The geometric mean of the two sides, not the smaller one: sizing
    // everything by the short side leaves the composition sitting in the
    // middle third of the window with black above and below it.
    const float s = std::sqrt(w * h);
    // ... and stretched vertically to match, so the orbits fill the frame
    // rather than describing a circle inside it.
    const float aspect = std::min(2.4f, std::max(1.0f, h / w));
    const float y_stretch = 0.55f + 0.42f * aspect;

    const MoodPalette& p = state.palette;
    const ne_vis& v = state.vis;
    const float gate = v.gate;

    // The composition follows the pointer, but only a third of the way.
    // Tracking it exactly throws everything into the corner being reached for
    // and leaves two thirds of the window dead black. The field is expressed
    // by colour, size and spread - it does not also need translation.
    auto place = [&](Vec2 n) {
        return Vec2{(0.5f + (n.x - 0.5f) * 0.30f) * w,
                    (0.5f + (n.y - 0.5f) * 0.26f) * h};
    };
    const Vec2 centre = place(state.centre);

    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColorFloat(r_, p.bg.r, p.bg.g, p.bg.b, 1.0f);
    SDL_RenderClear(r_);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_ADD);

    // A pool of light under the field, so the composition has a place to sit
    // even when every voice is quiet.
    blob(centre, s * 1.15f, p.deep.with_alpha(0.26f + 0.12f * gate));

    // ---- the register slices ---------------------------------------------
    for (int i = 0; i < NE_BANDS; ++i) {
        const float t = (float)i / (float)(NE_BANDS - 1);
        float energy = v.band[i];
        // While paused the engine reports silence, but a black window is not a
        // pause, it is a crash - and this is the first thing anyone sees. So
        // the field breathes on its own until the music arrives. Multiplied by
        // (1 - gate), so the moment sound starts this contributes nothing and
        // the picture is the synthesizer's again.
        const float idle =
            (0.26f + 0.11f * std::sin(state.t * 0.21f + (float)i * 1.7f)) * (1.0f - gate);
        energy = std::max(energy, idle);

        const float orbit_r = s * (0.06f + 0.42f * std::pow(t, 0.85f));
        const Vec2 o = state.orbit(i, orbit_r);
        const Vec2 pos{centre.x + o.x, centre.y + o.y * y_stretch};
        // Low slices are big and soft, high ones small and sharp - which is
        // what the register actually sounds like.
        const float radius = s * (0.30f - 0.19f * t) * (0.45f + 1.05f * energy);
        blob(pos, radius,
             p.for_band(i, NE_BANDS, v.centroid)
                 .with_alpha(std::min(0.92f, 0.10f + 0.80f * energy)));
    }

    // ---- the drone itself -------------------------------------------------
    const float core = std::max(v.level, 0.24f * (1.0f - gate));
    blob(centre, s * (0.15f + 0.19f * core),
         lerp(p.deep, p.mid, 0.35f + 0.55f * v.centroid)
             .with_alpha(std::min(0.95f, 0.22f + 0.62f * core)));

    // ---- harmonic movement ------------------------------------------------
    for (const Ripple& rp : state.ripples()) {
        const float u = clampf01(rp.age / 7.0f);
        const float radius = s * (0.06f + 1.05f * ease_out_cubic(u));
        const float fade = (1.0f - u) * (1.0f - u);
        ring(centre, radius, 0.6f + 1.4f * fade, p.accent.with_alpha(0.085f * fade));
    }

    // ---- bells ------------------------------------------------------------
    for (const Spark& sp : state.sparks()) {
        const float u = clampf01(sp.age / 3.2f);
        const float fade = std::pow(1.0f - u, 1.8f);
        const Vec2 q = place(sp.pos);
        blob(q, s * 0.055f * sp.size * (0.4f + 1.6f * fade),
             p.accent.with_alpha(0.55f * fade));
        blob(q, s * 0.010f * sp.size, Colour{1, 1, 1, 0.85f * fade});
    }

    // ---- the pointer's wake ------------------------------------------------
    const std::vector<TrailPoint>& tr = state.trail();
    if (tr.size() > 1) {
        for (size_t i = 1; i < tr.size(); ++i) {
            const float fade = clampf01(1.0f - tr[i].age / 1.1f);
            if (fade <= 0.01f) continue;
            line(Vec2{tr[i - 1].p.x * w, tr[i - 1].p.y * h},
                 Vec2{tr[i].p.x * w, tr[i].p.y * h}, 1.0f + 5.0f * fade * fade,
                 p.accent.with_alpha(0.16f * fade));
        }
        const TrailPoint& head = tr.back();
        const float fade = clampf01(1.0f - head.age / 1.1f);
        blob(Vec2{head.p.x * w, head.p.y * h}, s * 0.06f * fade,
             p.accent.with_alpha(0.28f * fade));
    }

    // ---- the invitation ---------------------------------------------------
    if (state.idle_hint > 0.01f) {
        const float breathe = 0.5f + 0.5f * std::sin(state.t * 0.9f);
        ring(centre, s * (0.115f + 0.012f * breathe), 1.1f,
             p.accent.with_alpha(state.idle_hint * (0.24f + 0.16f * breathe)));
        const float d = s * 0.030f;
        triangle(Vec2{centre.x - d * 0.45f, centre.y - d},
                 Vec2{centre.x - d * 0.45f, centre.y + d},
                 Vec2{centre.x + d * 0.95f, centre.y},
                 p.accent.with_alpha(state.idle_hint * 0.55f));
    }
}

}  // namespace ne
