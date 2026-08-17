#include "textures.h"

#include <cmath>

#include "dsp.h"

namespace ne {

TextureData make_blob(int size) {
    TextureData t;
    t.width = t.height = size;
    t.rgba.assign((size_t)size * size * 4, 0);
    const float c = (float)(size - 1) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = ((float)x - c) / c;
            const float dy = ((float)y - c) / c;
            const float r = std::sqrt(dx * dx + dy * dy);
            float a = 0.0f;
            if (r < 1.0f) {
                const float f = 0.5f + 0.5f * std::cos(kPi * r);
                a = f * f * (0.55f + 0.45f * f);
            }
            const size_t i = ((size_t)y * size + x) * 4;
            t.rgba[i + 0] = 255;
            t.rgba[i + 1] = 255;
            t.rgba[i + 2] = 255;
            int q = (int)std::lround(a * 255.0f);
            t.rgba[i + 3] = (uint8_t)(q < 0 ? 0 : (q > 255 ? 255 : q));
        }
    }
    return t;
}

TextureData make_grain(int size) {
    TextureData t;
    t.width = t.height = size;
    t.rgba.assign((size_t)size * size * 4, 0);
    // The engine's own PCG32 rather than the standard library's, so the grain
    // is identical on every platform. The pattern differs from the Dart
    // version's - a different generator - which does not matter for noise.
    Rng rng;
    rng.seed(0x4E756C6Cu, 1);
    for (int i = 0; i < size * size; ++i) {
        const uint8_t v = (uint8_t)rng.range(4);
        t.rgba[(size_t)i * 4 + 0] = v;
        t.rgba[(size_t)i * 4 + 1] = v;
        t.rgba[(size_t)i * 4 + 2] = v;
        t.rgba[(size_t)i * 4 + 3] = 255;
    }
    return t;
}

}  // namespace ne
