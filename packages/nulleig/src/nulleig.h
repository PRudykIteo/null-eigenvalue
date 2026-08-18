/* nulleig.h - the whole engine, as flat C so Dart's FFI can call it.
 *
 * There is one object, `ne_engine`, and it owns everything: the synthesis, the
 * effects and the audio device. Dart never sees a sample. It sets a handful of
 * scalars and reads back a small block of numbers to draw with.
 *
 * Threading contract, which is the only subtle thing here:
 *   - ne_create / ne_destroy / ne_start / ne_stop are called from one thread
 *     (Dart's) and are not reentrant.
 *   - every ne_set_* and ne_get_vis is safe to call from any thread at any
 *     time. They touch nothing but atomics, so the audio callback can never be
 *     blocked by a UI that is busy laying out a frame. This is why the setters
 *     take plain floats rather than a struct pointer: a struct copy would need
 *     a lock or a sequence counter, and the parameter set is small enough that
 *     per-field atomics are simpler and strictly better.
 *   - ne_render is called from the audio thread (or from a test harness) and
 *     allocates nothing.
 */
#ifndef NULLEIG_H
#define NULLEIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// `used` as well as `visible`. Dart resolves these at run time rather than by
// linking against them, so to the linker they look like symbols nobody calls -
// and a dead-strip would throw the entire engine away, producing a build that
// succeeds and then cannot find ne_create.
#if defined(_WIN32)
#define NE_API __declspec(dllexport)
#else
#define NE_API __attribute__((visibility("default"))) __attribute__((used))
#endif

/* Number of aura bands published for the visuals. One per register slice; the
 * UI turns them into the drifting orbs. */
#define NE_BANDS 8

typedef struct ne_engine ne_engine;

/* What the UI needs to draw a picture of what it is hearing. Everything is
 * already smoothed for display - the caller can use these values raw at 60 Hz
 * without filtering them again. */
typedef struct ne_vis {
    float level;              /* 0..1, smoothed peak of the master bus       */
    float centroid;           /* 0..1, brightness (log spectral centre)      */
    float band[NE_BANDS];     /* 0..1, energy per register slice, low first  */
    float spark;              /* 0..1, decays after each bell strike         */
    float root_hz;            /* current drone fundamental                   */
    float motion;             /* 0..1, how much the harmony is moving now    */
    float gate;               /* 0..1, master fade (0 paused, 1 playing)     */
    int   chord_change;       /* increments on every harmonic move           */
} ne_vis;

/* ---------------------------------------------------------------- lifecycle */

/* `sample_rate` may be 0, meaning "whatever the device wants" - the real rate
 * is then decided by ne_start(). Pass a real rate when rendering offline. */
NE_API ne_engine* ne_create(int sample_rate);
NE_API void ne_destroy(ne_engine* e);

/* Opens the sound card and starts pulling audio. Returns 0 on success.
 * A no-op that returns 0 if the device is already running. */
NE_API int ne_start(ne_engine* e);
NE_API void ne_stop(ne_engine* e);

/* The actual sample rate in use (after ne_start may differ from the request). */
NE_API int ne_sample_rate(const ne_engine* e);

/* Interleaved stereo. `frames` is the per-channel count. Used by the audio
 * callback and by the offline harness; safe to call directly when the device
 * is not running. */
NE_API void ne_render(ne_engine* e, float* out, int frames);

/* Jump to `seconds` into the piece, without producing the audio on the way.
 *
 * A piece has no end - the voices breathe on golden-ratio periods so the
 * combination does not recur for days - which makes "I heard something good
 * half an hour in" a real problem: there is nothing to rewind. This is the
 * answer to it. Everything that decides what the piece *is* happens at control
 * rate, 750 times a second, so a skip runs those blocks and leaves the
 * per-sample DSP alone.
 *
 * What this gives you is the same music: the same pitches at the same moments,
 * the same bells, the same weather. What it does not give you is the same
 * waveform down to the sample - the reverb and the delay are their own
 * history, and although the last thirty seconds are rendered properly to fill
 * them, arriving here in real time would have left slightly different tails.
 * For finding a place again that difference is inaudible; for a bit-exact
 * comparison it is not, so the render harness measures from zero.
 *
 * Measured: half an hour of music arrives in 1.9 s instead of 38 s, with the
 * same root, the same number of harmonic moves and the same RMS to within one
 * per cent. Blocks the calling thread, so do not call it from the audio
 * callback. */
NE_API void ne_skip(ne_engine* e, double seconds);

/* ---------------------------------------------------------------- parameters */

#define NE_MOOD_COUNT 6

/* 0 Kernel, 1 Manifold, 2 Halo, 3 Torsion, 4 Limit, 5 Entropy. Clamped.
 * A change is not instant: the new mood's harmony is walked into over the next
 * minute or so rather than cut to, because a drone that jump-cuts is a
 * different piece rather than the same one in a new light. */
NE_API void ne_set_mood(ne_engine* e, int mood);
NE_API int  ne_mood(const ne_engine* e);

/* The 2D field, both 0..1. x is brightness (dark .. airy), y is density
 * (sparse and low .. dense and wide). Smoothed internally over seconds. */
NE_API void ne_set_field(ne_engine* e, float x, float y);

/* The finger itself, as distinct from where it left the field: `speed` is in
 * screen-widths per second and feeds a short excitation that makes fast moves
 * audible as shimmer rather than only as a parameter change. */
NE_API void ne_set_touch(ne_engine* e, int active, float speed);

/* Master fade. Anything non-zero fades up over ~1.2 s, zero fades down and
 * then idles the synthesis. The device keeps running either way - reopening it
 * on every pause is how you collect glitches on the way back in.
 *
 * Once the fade is out, the piece itself stops: the harmony, the weather and
 * every voice hold where they were, and ne_elapsed stops with them. A pause is
 * a place in the music you can come back to, not a mute over a piece that
 * carries on ageing without you. The sleep timer is the exception and keeps
 * counting - see ne_set_sleep. */
NE_API void ne_set_playing(ne_engine* e, int playing);
NE_API int  ne_playing(const ne_engine* e);

/* 0..1 linear, applied last. */
NE_API void ne_set_gain(ne_engine* e, float gain);

/* Sleep timer. Arms a point `seconds` of device time from now; the last
 * twenty seconds before it are a fade rather than a countdown to a cut, and
 * when it lands the engine clears its own playing flag. The deadline lives
 * here and not in Dart because it has to fire behind a locked screen, where
 * the UI may be suspended - the audio thread is the only clock this app can
 * trust to still be running. `seconds` <= 0 disarms. Counts down through a
 * manual pause (like a radio's sleep switch), so re-arming is never needed. */
NE_API void ne_set_sleep(ne_engine* e, double seconds);

/* Seconds until the armed sleep fires; negative when disarmed. */
NE_API double ne_sleep_remaining(const ne_engine* e);

/* Re-seeds every random stream and restarts the piece from silence: the
 * voices, the harmony, the bells, the reverb tail and every smoothed parameter
 * go back to a known state, so the same seed really does give the same music
 * rather than merely the same notes over whatever was already ringing.
 *
 * Note that the seed alone does not describe a piece. The mood decides the
 * scale, the register and half the effects; the field decides brightness and
 * density, and through them the filter, the timbre, how many voices are
 * sounding and how often bells arrive. Use ne_set_piece to set all four - this
 * one leaves the other three wherever they were. */
NE_API void ne_set_seed(ne_engine* e, uint32_t seed);

/* The seed currently in force. */
NE_API uint32_t ne_seed(const ne_engine* e);

/* Everything that decides what a piece is, applied as one thing.
 *
 * This exists because the parts cannot be set separately and mean the same
 * thing. The audio thread acts on a reseed at its next control block, and the
 * reseed draws the opening chord out of whatever mood is current at that
 * moment - so ne_set_mood followed by ne_set_seed pitches every voice from the
 * mood being left behind. `x` and `y` are the field, both 0..1, as
 * ne_set_field. */
NE_API void ne_set_piece(ne_engine* e, uint32_t seed, int mood, float x, float y);

/* ------------------------------------------------------------- diagnostics */

/* Why there is no sound, and what it costs to make it.
 *
 * Silence has several very different causes that all look identical from the
 * outside - a device that never opened, a callback that is never called, or a
 * synthesizer producing zeroes - and a downloaded build has no console to tell
 * them apart. So the engine reports enough to distinguish them, and the app
 * shows it.
 *
 * Read it like this: `callbacks` still 0 means the OS is not asking us for
 * audio, so the problem is the device or the session. `callbacks` rising with
 * `level` at 0 means the synthesizer is idle - check `gate`. Both non-zero and
 * still silence means the session is playing us into nothing. */
typedef struct ne_status {
    int started;            /* 1 once ne_start has succeeded              */
    int ma_context;         /* ma_result of ma_context_init, 0 = ok       */
    int ma_device_init;     /* ma_result of ma_device_init                */
    int ma_device_start;    /* ma_result of ma_device_start               */
    int device_state;       /* ma_device_get_state, -1 when uninitialised */
    int sample_rate;        /* what the device actually runs at           */
    unsigned int callbacks; /* audio callbacks served since ne_start      */
    double elapsed;         /* seconds of audio rendered                  */
} ne_status;

NE_API void ne_get_status(ne_engine* e, ne_status* out);

/* ------------------------------------------------------------- introspection */

NE_API void ne_get_vis(ne_engine* e, ne_vis* out);

/* Human-readable name of a mood, for the UI. Static storage, never null. */
NE_API const char* ne_mood_name(int mood);

/* Seconds of audio produced since ne_create. */
NE_API double ne_elapsed(const ne_engine* e);

#ifdef __cplusplus
}
#endif
#endif /* NULLEIG_H */
