// api.cpp - the flat C surface Dart calls, plus the audio device.
//
// The device lives here rather than on the Dart side on purpose: the render
// callback is the OS's audio thread talking straight to the synthesizer, with
// Dart nowhere in the path - it can be janked or garbage-collecting and the
// drone does not notice.

#include "nulleig.h"

#include <atomic>
#include <cstdlib>
#include <new>

#include "engine.h"

#ifdef NE_WITH_MINIAUDIO
#include <chrono>
#include <thread>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#endif

namespace {

constexpr int kDefaultRate = 48000;

struct Holder {
    ne::Engine engine;
    // Every step of opening the device records its result, because on a
    // sideloaded build there is no console and "no sound" has to be
    // diagnosable from inside the app.
    std::atomic<int> r_context{-999};
    std::atomic<int> r_device_init{-999};
    std::atomic<int> r_device_start{-999};
    std::atomic<int> started{0};
    std::atomic<unsigned int> callbacks{0};
    // Share of each buffer's own duration that render() spends producing it.
    // The number that decides whether "the app is CPU heavy" is a question
    // about the synthesis or about the picture - and until it is on screen,
    // that question gets answered by guessing.
    std::atomic<float> load{0.0f};
#ifdef NE_WITH_MINIAUDIO
    ma_context context{};
    ma_device device{};
    bool context_ok = false;
    bool device_ok = false;
    std::atomic<bool> want_running{false};
    std::atomic<bool> watchdog_quit{false};
    std::thread watchdog;
#endif
    explicit Holder(int sr) : engine(sr) {}
};

#ifdef NE_WITH_MINIAUDIO
void data_callback(ma_device* dev, void* out, const void* in, ma_uint32 frames) {
    (void)in;
    Holder* h = (Holder*)dev->pUserData;
    if (h == nullptr) return;
    h->callbacks.fetch_add(1, std::memory_order_relaxed);

    const auto t0 = std::chrono::steady_clock::now();
    h->engine.render((float*)out, (int)frames);
    const auto t1 = std::chrono::steady_clock::now();

    // A clock read either side of the render is about forty nanoseconds
    // against a twenty-millisecond buffer, so measuring this costs nothing
    // worth measuring.
    const double sr = (double)dev->sampleRate;
    if (sr > 0.0 && frames > 0) {
        const double spent =
            std::chrono::duration<double>(t1 - t0).count();
        const double budget = (double)frames / sr;
        const float now = (float)(spent / budget);
        // Smoothed over about a second of callbacks. An instantaneous figure
        // swings between 0.2% and 8% depending on where the control block
        // lands, which reads as a broken meter rather than as a load.
        float prev = h->load.load(std::memory_order_relaxed);
        h->load.store(prev + 0.05f * (now - prev), std::memory_order_relaxed);
    }
}

// The device is restarted from here rather than from the notification
// callback, which miniaudio explicitly forbids re-entering. A poll also covers
// the cases that arrive without a notification at all - a route change that
// silently stops the unit, an interruption that ends while the app is
// suspended - so it is the more honest mechanism anyway.
void watchdog_main(Holder* h) {
    while (!h->watchdog_quit.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        if (!h->want_running.load(std::memory_order_relaxed)) continue;
        if (!h->device_ok) continue;
        // Only from `stopped`, not from "anything that is not started".
        // `starting` and `stopping` are transient states the backend passes
        // through on its own, and restarting a device that is mid-start turns
        // a working device into a permanent stutter.
        if (ma_device_get_state(&h->device) == ma_device_state_stopped) {
            ma_device_start(&h->device);  // fails harmlessly during a call
        }
    }
}
#endif

}  // namespace

extern "C" {

NE_API ne_engine* ne_create(int sample_rate) {
    int sr = sample_rate > 8000 ? sample_rate : kDefaultRate;
    Holder* h = new (std::nothrow) Holder(sr);
    return (ne_engine*)h;
}

NE_API void ne_destroy(ne_engine* e) {
    Holder* h = (Holder*)e;
    if (!h) return;
#ifdef NE_WITH_MINIAUDIO
    h->want_running.store(false, std::memory_order_relaxed);
    h->watchdog_quit.store(true, std::memory_order_relaxed);
    if (h->watchdog.joinable()) h->watchdog.join();
    if (h->device_ok) ma_device_uninit(&h->device);
    if (h->context_ok) ma_context_uninit(&h->context);
#endif
    delete h;
}

NE_API int ne_start(ne_engine* e) {
#ifdef NE_WITH_MINIAUDIO
    Holder* h = (Holder*)e;
    if (!h) return -1;
    if (h->device_ok && ma_device_get_state(&h->device) == ma_device_state_started) {
        h->want_running.store(true, std::memory_order_relaxed);
        return 0;
    }

    if (!h->context_ok) {
        ma_context_config cfg = ma_context_config_init();
        int r = (int)ma_context_init(nullptr, 0, &cfg, &h->context);
        h->r_context.store(r, std::memory_order_relaxed);
        if (r != MA_SUCCESS) return -2;
        h->context_ok = true;
    }

    if (!h->device_ok) {
        ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
        cfg.playback.format = ma_format_f32;
        cfg.playback.channels = 2;
        cfg.sampleRate = (ma_uint32)h->engine.sample_rate();
        cfg.dataCallback = data_callback;
        cfg.pUserData = h;
        // ~20 ms. Long enough that a scheduling hiccup cannot starve it, short
        // enough that dragging a finger still feels connected to the sound.
        cfg.periodSizeInMilliseconds = 20;
        cfg.performanceProfile = ma_performance_profile_conservative;
        int r = (int)ma_device_init(&h->context, &cfg, &h->device);
        h->r_device_init.store(r, std::memory_order_relaxed);
        if (r != MA_SUCCESS) return -3;
        h->device_ok = true;
    }

    int rs = (int)ma_device_start(&h->device);
    h->r_device_start.store(rs, std::memory_order_relaxed);
    if (rs != MA_SUCCESS) return -4;
    h->started.store(1, std::memory_order_relaxed);
    h->want_running.store(true, std::memory_order_relaxed);
    if (!h->watchdog.joinable()) {
        h->watchdog_quit.store(false, std::memory_order_relaxed);
        h->watchdog = std::thread(watchdog_main, h);
    }
    return 0;
#else
    (void)e;
    return -100;  // built without a device (offline harness)
#endif
}

NE_API void ne_stop(ne_engine* e) {
#ifdef NE_WITH_MINIAUDIO
    Holder* h = (Holder*)e;
    if (!h) return;
    h->want_running.store(false, std::memory_order_relaxed);
    if (h->device_ok) ma_device_stop(&h->device);
#else
    (void)e;
#endif
}

NE_API int ne_sample_rate(const ne_engine* e) {
    const Holder* h = (const Holder*)e;
    return h ? h->engine.sample_rate() : 0;
}

NE_API void ne_render(ne_engine* e, float* out, int frames) {
    Holder* h = (Holder*)e;
    if (!h || !out || frames <= 0) return;
    h->engine.render(out, frames);
}

NE_API void ne_set_mood(ne_engine* e, int mood) {
    if (e) ((Holder*)e)->engine.set_mood(mood);
}
NE_API int ne_mood(const ne_engine* e) {
    return e ? ((const Holder*)e)->engine.mood() : 0;
}
NE_API void ne_set_field(ne_engine* e, float x, float y) {
    if (e) ((Holder*)e)->engine.set_field(x, y);
}
NE_API void ne_set_touch(ne_engine* e, int active, float speed) {
    if (e) ((Holder*)e)->engine.set_touch(active != 0, speed);
}
NE_API void ne_set_playing(ne_engine* e, int playing) {
    if (e) ((Holder*)e)->engine.set_playing(playing != 0);
}
NE_API int ne_playing(const ne_engine* e) {
    return e ? (((const Holder*)e)->engine.playing() ? 1 : 0) : 0;
}
NE_API void ne_set_gain(ne_engine* e, float gain) {
    if (e) ((Holder*)e)->engine.set_gain(gain);
}
NE_API void ne_set_seed(ne_engine* e, uint32_t seed) {
    if (e) ((Holder*)e)->engine.set_seed(seed);
}
NE_API uint32_t ne_seed(const ne_engine* e) {
    return e ? ((const Holder*)e)->engine.seed() : 0u;
}
NE_API void ne_set_piece(ne_engine* e, uint32_t seed, int mood, float x, float y) {
    if (!e) return;
    if (mood < 0) mood = 0;
    if (mood >= NE_MOOD_COUNT) mood = NE_MOOD_COUNT - 1;
    ((Holder*)e)->engine.set_piece(seed, mood, x, y);
}
NE_API void ne_set_sleep(ne_engine* e, double seconds) {
    if (e) ((Holder*)e)->engine.set_sleep(seconds);
}
NE_API double ne_sleep_remaining(const ne_engine* e) {
    return e ? ((const Holder*)e)->engine.sleep_remaining() : -1.0;
}
NE_API void ne_get_vis(ne_engine* e, ne_vis* out) {
    if (e && out) ((Holder*)e)->engine.get_vis(out);
}

NE_API void ne_get_status(ne_engine* e, ne_status* out) {
    if (!out) return;
    Holder* h = (Holder*)e;
    if (!h) {
        *out = ne_status{};
        return;
    }
    out->started = h->started.load(std::memory_order_relaxed);
    out->ma_context = h->r_context.load(std::memory_order_relaxed);
    out->ma_device_init = h->r_device_init.load(std::memory_order_relaxed);
    out->ma_device_start = h->r_device_start.load(std::memory_order_relaxed);
    out->callbacks = h->callbacks.load(std::memory_order_relaxed);
    out->elapsed = h->engine.elapsed();
    out->load = h->load.load(std::memory_order_relaxed);
    out->sample_rate = h->engine.sample_rate();
#ifdef NE_WITH_MINIAUDIO
    out->device_state = h->device_ok ? (int)ma_device_get_state(&h->device) : -1;
    if (h->device_ok) out->sample_rate = (int)h->device.sampleRate;
#else
    out->device_state = -1;
#endif
}
NE_API const char* ne_mood_name(int mood) { return ne::mood_at(mood).name; }
NE_API double ne_elapsed(const ne_engine* e) {
    return e ? ((const Holder*)e)->engine.elapsed() : 0.0;
}

}  // extern "C"
