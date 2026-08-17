// harmony.h - what makes this a piece rather than a chord.
//
// The problem with a generated drone is that the two obvious designs are both
// wrong. A fixed chord is a texture: put it on and in ninety seconds you have
// heard everything it will ever do. A chord *progression* is worse, because a
// loop that comes round every eight bars announces itself as a loop the second
// time you hear it.
//
// So there is no progression here. There is a fixed drone root, and above it a
// set of voices that each breathe in and out on their own period, and each one
// picks a new pitch every time it comes back in. Three things make that work:
//
//   - The breath periods are spaced by the golden ratio (24 s .. 86 s, no two
//     alike and no two commensurable), so the *combination* of which voices
//     are sounding has a recurrence time measured in days. Nothing about the
//     texture ever repeats, without a single random number being involved in
//     the timing.
//
//   - A voice only ever changes pitch while it is silent. Nothing has to
//     crossfade or glide, so the harmony can move as much as it likes and you
//     never hear a change happen - you notice, half a minute later, that the
//     chord is somewhere else.
//
//   - The new pitch is drawn from the mood's scale, weighted by how it sounds
//     against the voices that are currently audible (see `weigh`). That is the
//     part that keeps twelve independently-wandering voices reading as one
//     harmony instead of a cluster.
//
// Over a longer timescale the root itself walks, by a fifth or a third, every
// few minutes. Voices hold their offset, so the whole field transposes at once
// - the one event in the piece big enough to notice while it happens.
#pragma once

#include <cmath>

#include "dsp.h"

namespace ne {

constexpr int kMoodCount = 6;

// The mood field is three bits and only six values were ever used. Six means
// "not one of the named instruments - derive it from the piece's seed", which
// buys a continuous instrument space without touching the token format: every
// token written before this still names the same one of the six.
constexpr int kMoodGenerated = 6;

struct Mood {
    const char* name;
    uint16_t scale;       // pitch class bitmask, bit 0 = root
    int root_midi;        // where the drone sits
    float low_semi;       // register the walking voices may occupy,
    float high_semi;      // in semitones above the root
    float breath_scale;   // multiplies every voice's breath period
    float root_walk_sec;  // mean seconds between root shifts
    float morph;          // timbre baseline, 0 sine .. 3 glass
    float morph_span;     // how far the brightness axis moves it
    float rev_decay;      // RT60, seconds
    float rev_size;
    float rev_mix;
    float shimmer;
    float bell_per_min;   // PHRASES per minute at mid density; 0 disables them
    float bell_decay;     // seconds
    float drive;
    float chorus;
    float air;            // noise bed level
    float delay_sec;
    float delay_fb;
    float delay_mix;
    float tilt;           // -1 dark .. +1 bright
    float cutoff_lo;      // master filter sweep, Hz, at field x = 0
    float cutoff_hi;      // ... and at x = 1
};

// The six. They are not presets in the "starting point" sense - each is a
// different instrument, and switching is the only large gesture the UI has.
//
// Only Halo rings. Bells were originally sprinkled through every mood and they
// were wrong everywhere else: against a warm held chord a struck note reads as
// an interruption however quiet it is. Halo is bright and airy enough to carry
// them, so that is where they live.
inline const Mood& mood_at(int i) {
    static const Mood kMoods[kMoodCount] = {
        // Kernel - the null space. As low and as still as the thing goes.
        {"Kernel",
         0b0000010010001101,  // 0 2 3 7 10 : minor, no third-stacking, no 6th
         29, 0.0f, 38.0f, 1.7f, 330.0f,
         0.75f, 0.9f,
         19.0f, 1.15f, 0.42f, 0.04f,
         0.0f, 5.0f,
         0.22f, 0.20f, 0.07f,
         6.4f, 0.62f, 0.16f,
         -0.30f, 420.0f, 3400.0f},

        // Manifold - warm, wide, the default. Dorian, so it is minor without
        // being sad about it.
        {"Manifold",
         0b0000011010101101,  // 0 2 3 5 7 9 10
         33, 0.0f, 45.0f, 1.15f, 260.0f,
         1.50f, 1.1f,
         13.0f, 0.95f, 0.44f, 0.14f,
         0.0f, 3.4f,
         0.17f, 0.42f, 0.10f,
         4.2f, 0.55f, 0.22f,
         -0.05f, 750.0f, 8000.0f},

        // Halo - lydian, high, and the only mood with real shimmer. The #4 is
        // the whole character: it never resolves, so it can hang forever.
        {"Halo",
         0b0000101011010101,  // 0 2 4 6 7 9 11
         36, 0.0f, 50.0f, 0.95f, 220.0f,
         2.20f, 0.9f,
         24.0f, 1.25f, 0.52f, 0.62f,
         4.0f, 4.5f,
         0.10f, 0.55f, 0.09f,
         3.0f, 0.66f, 0.30f,
         0.30f, 1400.0f, 14000.0f},

        // Torsion - phrygian with a major third above a minor scale. Tense,
        // metallic, the shortest tail so the dissonance stays legible.
        {"Torsion",
         0b0000010110110011,  // 0 1 4 5 7 8 10
         31, 0.0f, 43.0f, 0.85f, 190.0f,
         2.50f, 0.8f,
         8.5f, 0.75f, 0.38f, 0.22f,
         0.0f, 2.2f,
         0.40f, 0.30f, 0.17f,
         2.25f, 0.70f, 0.32f,
         0.08f, 950.0f, 10000.0f},

        // Limit - the piece as it stops. Pentatonic, the fewest voices, the
        // longest breaths and a thirty-second room.
        {"Limit",
         0b0000010010101001,  // 0 3 5 7 10
         28, 0.0f, 41.0f, 2.4f, 420.0f,
         1.15f, 0.85f,
         30.0f, 1.35f, 0.58f, 0.34f,
         0.0f, 7.0f,
         0.09f, 0.36f, 0.12f,
         8.5f, 0.68f, 0.24f,
         -0.15f, 550.0f, 5200.0f},

        // Entropy - the classic drone: something hums, something hisses.
        // The noise bed is the instrument here and the pitched voices are the
        // accompaniment, which is the reverse of every other mood. The scale
        // is root, second and fifth only: no third, so there is no major or
        // minor to hear, just an open interval that can be held forever.
        {"Entropy",
         0b0000000010000101,  // 0 2 7
         26, 0.0f, 31.0f, 2.1f, 520.0f,
         0.35f, 0.60f,
         26.0f, 1.20f, 0.50f, 0.06f,
         0.0f, 5.0f,
         0.30f, 0.15f, 0.62f,
         7.0f, 0.50f, 0.12f,
         -0.15f, 300.0f, 6000.0f},
    };
    if (i < 0) i = 0;
    if (i >= kMoodCount) i = kMoodCount - 1;
    return kMoods[i];
}

// ------------------------------------------------------- generated instruments
//
// The six above are hand-tuned and they are good because somebody listened to
// them. This makes new ones, and the whole problem is that twenty parameters
// drawn independently produce noise with a pitch in it rather than an
// instrument.
//
// What keeps a generated one coherent is that the six are not six unrelated
// presets. Read as data they lie along three axes, and the correlations are
// consistent: the low instruments breathe slower, walk their root less often
// and sit under a darker filter; the vast ones have both the long tail and the
// long delay. So this draws four latent values and derives the twenty, rather
// than drawing twenty.
//
//   weight   low, slow and dark .. high, quick and bright
//   space    dry and close .. a room you could lose something in
//   grain    pure tone .. the noise bed is the instrument
//   tension  open fifths .. phrygian with a major third in it
//
// Two things come from lists rather than ranges, because a spectrum analysis
// of the existing moods says the audible partials *are* the scale mask: a
// random twelve-bit mask is a detuned cluster, not a mode. So the scale comes
// from real modes ordered by tension, and the root lands on a whole MIDI note.
struct ScaleFamily {
    uint16_t mask;
    const char* name;
};

// Ordered by tension, which is what `tension` indexes into. The first is two
// notes and an octave of room; the last is Torsion's.
inline const ScaleFamily* scale_families(int* count) {
    static const ScaleFamily kFamilies[] = {
        {0b0000000010000101, "open"},        // 0 2 7
        {0b0000010010101001, "pentatonic"},  // 0 3 5 7 10
        {0b0000001010100101, "pentatonic"},  // 0 2 5 7 9
        {0b0000101011010101, "lydian"},      // 0 2 4 6 7 9 11
        {0b0000101010110101, "ionian"},      // 0 2 4 5 7 9 11
        {0b0000011010101101, "dorian"},      // 0 2 3 5 7 9 10
        {0b0000011010110101, "mixolydian"},  // 0 2 4 5 7 9 10
        {0b0000010010001101, "minor"},       // 0 2 3 7 10
        {0b0000010110101101, "aeolian"},     // 0 2 3 5 7 8 10
        {0b0000100100101101, "harmonic"},    // 0 2 3 5 7 8 11
        {0b0000010110101011, "phrygian"},    // 0 1 3 5 7 8 10
        {0b0000010110110011, "torsion"},     // 0 1 4 5 7 8 10
    };
    *count = (int)(sizeof(kFamilies) / sizeof(kFamilies[0]));
    return kFamilies;
}

// Where each named mood sits on the four axes. Used only to describe a
// generated instrument by what it is near - the six themselves are the literal
// data above and are not regenerated from this.
struct MoodAnchor {
    const char* name;
    float weight, space, grain, tension;
};

inline const MoodAnchor* mood_anchors() {
    static const MoodAnchor kAnchors[kMoodCount] = {
        {"Kernel", 0.25f, 0.45f, 0.25f, 0.55f},
        {"Manifold", 0.55f, 0.30f, 0.25f, 0.40f},
        {"Halo", 0.90f, 0.65f, 0.20f, 0.25f},
        {"Torsion", 0.45f, 0.10f, 0.40f, 0.95f},
        {"Limit", 0.15f, 0.90f, 0.20f, 0.12f},
        {"Entropy", 0.05f, 0.70f, 0.95f, 0.05f},
    };
    return kAnchors;
}

struct Latent {
    float weight = 0.5f, space = 0.5f, grain = 0.5f, tension = 0.5f;
};

inline Latent latent_for(uint32_t seed) {
    Rng r;
    r.seed(seed, 131);
    Latent l;
    // Uniform, including the corners. An earlier version pulled these toward
    // the middle on the theory that the extremes were where nobody had
    // listened - but a survey of 120 instruments rendered at the corners found
    // no clipping, no NaN and nothing silent, so the caution was buying
    // nothing and costing the variety that is the whole point.
    l.weight = r.uni();
    l.space = r.uni();
    // The one axis that stays timid, and on evidence rather than nerves: of
    // the six hand-tuned moods exactly one is a noise instrument. Squaring
    // keeps that ratio without putting the loud end out of reach.
    l.grain = r.uni() * r.uni();
    l.tension = r.uni();
    return l;
}

inline Mood mood_from_latent(const Latent& l, uint32_t seed) {
    Rng r;
    r.seed(seed, 149);
    // A little independent wobble on top of the correlated derivation, so two
    // instruments with the same weight are not the same instrument.
    auto jit = [&r](float amount) { return 1.0f + r.bi() * amount; };

    const float w = l.weight, sp = l.space, g = l.grain, t = l.tension;

    Mood m{};
    m.name = "";  // a generated instrument is described, not named

    int n = 0;
    const ScaleFamily* fam = scale_families(&n);
    int idx = (int)(t * (float)n);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    m.scale = fam[idx].mask;

    // A whole MIDI note. The roots of the six span D1 to C2 and there is no
    // reason to leave that register: below it the fundamental stops being a
    // pitch, above it the thing is no longer a drone.
    m.root_midi = 25 + (int)std::lround(12.0f * w);

    m.low_semi = 0.0f;
    m.high_semi = (30.0f + 21.0f * w) * jit(0.05f);

    // Low instruments breathe slower and move their root less often. This is
    // the strongest correlation in the six and the most obviously right one: a
    // fast low drone is a rumble.
    m.breath_scale = (2.6f - 1.7f * w) * jit(0.10f);
    m.root_walk_sec = (560.0f - 350.0f * w) * jit(0.12f);

    m.morph = (0.3f + 2.0f * w) * jit(0.08f);
    m.morph_span = 0.85f * jit(0.15f);

    m.cutoff_lo = 280.0f * std::pow(2.0f, 2.4f * w) * jit(0.10f);
    m.cutoff_hi = 3000.0f * std::pow(2.0f, 2.3f * w) * jit(0.10f);
    m.tilt = -0.32f + 0.62f * w + r.bi() * 0.06f;

    m.rev_decay = (7.0f + 25.0f * sp) * jit(0.10f);
    m.rev_size = (0.72f + 0.65f * sp) * jit(0.06f);
    m.rev_mix = (0.36f + 0.24f * sp) * jit(0.06f);
    m.delay_sec = (2.0f + 6.8f * sp) * jit(0.10f);
    m.delay_fb = (0.50f + 0.22f * sp) * jit(0.05f);
    m.delay_mix = (0.12f + 0.20f * sp) * jit(0.10f);

    // Shimmer needs both height and space: pitch-shifted feedback under a low
    // dark drone is a growl, not a halo.
    m.shimmer = 0.03f + 0.70f * sp * w * jit(0.15f);

    m.air = (0.05f + 0.60f * g) * jit(0.12f);
    m.drive = (0.08f + 0.35f * g) * jit(0.15f);
    // A noisy instrument needs less chorus; there is already movement in it.
    m.chorus = (0.50f - 0.35f * g) * jit(0.12f);

    // Bells only where Halo lives: high, clean and roomy. The finding they came
    // from is in the comment above kMoods and a continuous space does not
    // repeal it - against a warm held chord a struck note is an interruption
    // however quiet it is. Even in the right region they are the exception.
    m.bell_per_min = 0.0f;
    m.bell_decay = (2.0f + 5.0f * sp) * jit(0.10f);
    if (w > 0.55f && g < 0.45f && sp > 0.30f && r.uni() < 0.55f) {
        m.bell_per_min = 2.5f + 3.0f * r.uni();
    }

    return m;
}

// The instrument a seed describes, when a piece asks for a generated one
// rather than one of the six.
inline Mood generated_mood(uint32_t seed) {
    return mood_from_latent(latent_for(seed), seed);
}

// What to call it. A generated instrument has no name, so it is described by
// the anchors it is nearest: "HALO" when it is close to one, "MANIFOLD / HALO"
// when it sits between two. Honest, and it says something about what is about
// to be heard rather than pretending to be a preset.
//
// Writes into `out` and always NUL-terminates.
inline void describe_instrument(uint32_t seed, char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = '\0';
    const Latent l = latent_for(seed);
    const MoodAnchor* a = mood_anchors();

    int best = 0, second = 1;
    float bd = 1e9f, sd = 1e9f;
    for (int i = 0; i < kMoodCount; ++i) {
        const float dw = l.weight - a[i].weight;
        const float ds = l.space - a[i].space;
        const float dg = l.grain - a[i].grain;
        const float dt = l.tension - a[i].tension;
        // Weight counts double: it is the axis the ear notices first.
        const float d = 2.0f * dw * dw + ds * ds + dg * dg + dt * dt;
        if (d < bd) {
            sd = bd;
            second = best;
            bd = d;
            best = i;
        } else if (d < sd) {
            sd = d;
            second = i;
        }
    }

    int k = 0;
    const char* p = a[best].name;
    while (*p && k < cap - 1) out[k++] = *p++;
    // Close enough to one anchor to just be called that.
    if (bd > 0.020f && k < cap - 4) {
        out[k++] = ' ';
        out[k++] = '/';
        out[k++] = ' ';
        p = a[second].name;
        while (*p && k < cap - 1) out[k++] = *p++;
    }
    out[k] = '\0';
}


inline float midi_hz(float midi) {
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

// How well two pitches sit together, by interval class. Derived from the usual
// consonance ordering and then flattened a little: this is a weight in a
// random draw, not a rule, and a table with zeros in it produces a harmony
// that only ever states the same four notes.
inline float interval_weight(int semitones) {
    static const float kW[12] = {
        1.00f,  // unison / octave
        0.10f,  // minor 2nd
        0.42f,  // major 2nd
        0.90f,  // minor 3rd
        0.88f,  // major 3rd
        0.92f,  // perfect 4th
        0.30f,  // tritone
        1.00f,  // perfect 5th
        0.78f,  // minor 6th
        0.80f,  // major 6th
        0.62f,  // minor 7th
        0.24f,  // major 7th
    };
    int ic = semitones % 12;
    if (ic < 0) ic += 12;
    return kW[ic];
}

inline bool in_scale(uint16_t mask, int semitone_offset) {
    int pc = semitone_offset % 12;
    if (pc < 0) pc += 12;
    return (mask >> pc) & 1u;
}

// Every scale offset available to a walking voice, low to high.
struct PitchSet {
    static constexpr int kMax = 64;
    int offset[kMax];
    int count = 0;

    void build(const Mood& m) {
        count = 0;
        int lo = (int)m.low_semi, hi = (int)m.high_semi;
        for (int o = lo; o <= hi && count < kMax; ++o) {
            if (in_scale(m.scale, o)) offset[count++] = o;
        }
    }
};

// Picks the offset a voice should fade in on.
//
// `sounding` holds the absolute offsets of the voices currently audible and
// `amp` how audible each is; a voice at 5% volume should barely constrain the
// choice, which is what keeps the harmony from locking up as everything fades
// together. `centre`/`spread` are the voice's own register preference, so the
// ensemble stays spread across four octaves instead of collecting in the
// middle where the weights are happiest.
inline int choose_offset(const PitchSet& set, const int* sounding,
                         const float* amp, int n_sounding, float centre,
                         float spread, int previous, Rng& rng) {
    if (set.count == 0) return 0;

    float w[PitchSet::kMax];
    float total = 0.0f;
    for (int i = 0; i < set.count; ++i) {
        int cand = set.offset[i];

        // Consonance against what is audible, as a weighted geometric mean so
        // that adding a quiet voice cannot swamp the loud ones.
        float logsum = 0.0f, wsum = 0.0f;
        bool doubled = false;
        for (int j = 0; j < n_sounding; ++j) {
            float a = amp[j];
            if (a < 0.02f) continue;
            int d = cand - sounding[j];
            if (d < 0) d = -d;
            if (d == 0) doubled = true;
            logsum += a * std::log(interval_weight(d) + 1e-4f);
            wsum += a;
        }
        float consonance = wsum > 1e-4f ? std::exp(logsum / wsum) : 1.0f;

        // Register fit: a soft window, not a hard band, so a voice can stray
        // an octave when the weights strongly want it to.
        float d = ((float)cand - centre) / spread;
        float fit = std::exp(-0.5f * d * d);

        // Some doubling is the sound of an organ; a lot of it is a thin chord
        // played four times.
        float dup = doubled ? 0.30f : 1.0f;

        // Prefer not to land exactly where this voice was last time - the same
        // voice returning to the same note is the one way this design can read
        // as a loop.
        float novelty = (cand == previous) ? 0.15f : 1.0f;

        float weight = consonance * consonance * fit * dup * novelty;
        w[i] = weight;
        total += weight;
    }
    if (total <= 1e-9f) return set.offset[rng.range(set.count)];

    float r = rng.uni() * total;
    for (int i = 0; i < set.count; ++i) {
        r -= w[i];
        if (r <= 0.0f) return set.offset[i];
    }
    return set.offset[set.count - 1];
}

}  // namespace ne
