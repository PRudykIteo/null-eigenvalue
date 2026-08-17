#include "text.h"

#include <cmath>
#include <cstring>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

#include "font_data.h"

namespace ne {
namespace {

// Printable ASCII, which is exactly what the fonts were subset to.
constexpr int kFirst = 32;
constexpr int kLast = 126;
constexpr int kGlyphs = kLast - kFirst + 1;

// One row of glyphs; at these sizes the whole alphabet fits comfortably.
constexpr int kAtlasW = 1024;

}  // namespace

struct TextRenderer::Atlas {
    Weight weight = Weight::Light;
    float px = 0;
    SDL_Texture* tex = nullptr;
    stbtt_bakedchar chars[kGlyphs] = {};
    float ascent = 0;
    float cap = 0;
};

bool TextRenderer::init(SDL_Renderer* r) {
    r_ = r;
    return r_ != nullptr;
}

void TextRenderer::shutdown() {
    for (int i = 0; i < atlas_count_; ++i) {
        if (atlases_[i]) {
            if (atlases_[i]->tex) SDL_DestroyTexture(atlases_[i]->tex);
            delete atlases_[i];
        }
        atlases_[i] = nullptr;
    }
    atlas_count_ = 0;
}

TextRenderer::Atlas* TextRenderer::atlas_for(Weight w, float px) {
    // Quantised, so a continuously changing scale does not rasterise a new
    // alphabet every frame.
    const float q = std::round(px * 2.0f) * 0.5f;
    for (int i = 0; i < atlas_count_; ++i) {
        if (atlases_[i]->weight == w && atlases_[i]->px == q) return atlases_[i];
    }
    if (atlas_count_ >= kMaxAtlases) {
        // The window has been resized past anything this cache was meant for.
        // Dropping the oldest is better than refusing to draw the HUD.
        if (atlases_[0]->tex) SDL_DestroyTexture(atlases_[0]->tex);
        delete atlases_[0];
        for (int i = 1; i < atlas_count_; ++i) atlases_[i - 1] = atlases_[i];
        --atlas_count_;
    }

    const unsigned char* font = (w == Weight::Light) ? kInterLight : kInterRegular;

    const int h = (int)std::ceil(q * 2.0f) + 8;
    std::vector<unsigned char> bitmap((size_t)kAtlasW * h, 0);
    Atlas* a = new Atlas();
    a->weight = w;
    a->px = q;
    if (stbtt_BakeFontBitmap(font, 0, q, bitmap.data(), kAtlasW, h, kFirst, kGlyphs,
                             a->chars) <= 0) {
        // A negative return still bakes what fit; only a hard zero is fatal.
    }

    // stb bakes coverage; the HUD wants white glyphs whose alpha is that
    // coverage, so the tint can be applied per draw like every other element.
    std::vector<unsigned char> rgba((size_t)kAtlasW * h * 4);
    for (size_t i = 0; i < (size_t)kAtlasW * h; ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }
    a->tex = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                               kAtlasW, h);
    SDL_UpdateTexture(a->tex, nullptr, rgba.data(), kAtlasW * 4);
    // Blended, not added: lettering over a bright orb has to stay legible, and
    // additive white on white is invisible.
    SDL_SetTextureBlendMode(a->tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(a->tex, SDL_SCALEMODE_LINEAR);

    int asc = 0, desc = 0, gap = 0;
    stbtt_fontinfo info;
    stbtt_InitFont(&info, font, stbtt_GetFontOffsetForIndex(font, 0));
    stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
    const float sc = stbtt_ScaleForPixelHeight(&info, q);
    a->ascent = (float)asc * sc;
    int x0, y0, x1, y1;
    // 'H' rather than the font's declared cap height: it is the letter the
    // design actually lines things up against.
    if (stbtt_GetCodepointBox(&info, 'H', &x0, &y0, &x1, &y1)) {
        a->cap = (float)y1 * sc;
    } else {
        a->cap = a->ascent * 0.7f;
    }

    atlases_[atlas_count_++] = a;
    return a;
}

float TextRenderer::measure(const std::string& s, Weight w, float px, float tracking) {
    Atlas* a = atlas_for(w, px);
    float x = 0;
    for (unsigned char ch : s) {
        if (ch < kFirst || ch > kLast) continue;
        x += a->chars[ch - kFirst].xadvance + tracking;
    }
    // The trailing gap belongs after the last letter only when something
    // follows it; a centred line measured with it sits half a gap to the left.
    if (!s.empty()) x -= tracking;
    return x;
}

float TextRenderer::cap_height(Weight w, float px) { return atlas_for(w, px)->cap; }

void TextRenderer::draw(const std::string& s, float x, float y, Weight w, float px,
                        float tracking, Colour c) {
    if (c.a <= 0.002f) return;
    Atlas* a = atlas_for(w, px);
    SDL_SetTextureColorModFloat(a->tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaModFloat(a->tex, c.a);

    float pen = x;
    for (unsigned char ch : s) {
        if (ch < kFirst || ch > kLast) continue;
        const stbtt_bakedchar& b = a->chars[ch - kFirst];
        const float qw = (float)(b.x1 - b.x0);
        const float qh = (float)(b.y1 - b.y0);
        if (qw > 0 && qh > 0) {
            const SDL_FRect src{(float)b.x0, (float)b.y0, qw, qh};
            const SDL_FRect dst{pen + b.xoff, y + b.yoff, qw, qh};
            SDL_RenderTexture(r_, a->tex, &src, &dst);
        }
        pen += b.xadvance + tracking;
    }
}

void TextRenderer::draw_centred(const std::string& s, float cx, float y, Weight w,
                                float px, float tracking, Colour c) {
    draw(s, cx - measure(s, w, px, tracking) * 0.5f, y, w, px, tracking, c);
}

}  // namespace ne
