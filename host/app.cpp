#include "app.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace ne {
namespace {

// Enough to be a library, few enough that the panel never becomes a list to
// scroll through looking for something.
constexpr size_t kMaxLiked = 64;

// Smoothstep. A linear crossfade between two palettes has a visible start and
// a visible stop; this one only has a middle.
inline float ease(float t) {
    const float u = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return u * u * (3.0f - 2.0f * u);
}

// Long enough to read as the light changing rather than as a screen being
// repainted, short enough that it is over before the ear notices the harmony
// has started moving too.
constexpr float kMoodBlendSeconds = 2.2f;

}  // namespace

App::App(ne_engine* engine) : engine_(engine) {
    mood_ = prev_mood_ = ne_mood(engine);
    ne_set_field(engine_, x_, y_);
}

void App::set_mood(int m) {
    if (m < 0) m = 0;
    if (m >= kPaletteCount) m = kPaletteCount - 1;
    if (m == mood_) return;
    prev_mood_ = mood_;
    blend_ = 0.0f;
    mood_ = m;
    ne_set_mood(engine_, m);
    save_prefs();
}

void App::set_playing(bool p) {
    if (p == playing_) return;
    playing_ = p;
    ne_set_playing(engine_, p ? 1 : 0);
}

void App::toggle() { set_playing(!playing_); }

void App::set_field(float x, float y, bool touching, float speed) {
    x_ = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
    y_ = y < 0.0f ? 0.0f : (y > 1.0f ? 1.0f : y);
    ne_set_field(engine_, x_, y_);
    ne_set_touch(engine_, touching ? 1 : 0, speed);
}

void App::tick(float dt) {
    if (blend_ < 1.0f) {
        blend_ += dt / kMoodBlendSeconds;
        if (blend_ > 1.0f) blend_ = 1.0f;
    }
    // The engine can stop itself - a sleep timer landing - and the transport
    // should follow rather than claim to be playing silence.
    if (playing_ && ne_playing(engine_) == 0) playing_ = false;
}

MoodPalette App::palette() const {
    return palette_lerp(palette_at(prev_mood_), palette_at(mood_), ease(blend_));
}

// ------------------------------------------------------------------- pieces

Piece App::piece() const {
    return Piece{seed_, mood_, x_, y_}.quantised();
}

void App::apply_piece(const Piece& p) {
    const Piece q = p.quantised();
    seed_ = q.seed;
    prev_mood_ = mood_;
    if (q.mood != mood_) blend_ = 0.0f;
    mood_ = q.mood;
    x_ = q.x;
    y_ = q.y;
    // One call rather than four, because the engine reads the reseed ticket
    // before it reads the mood: setting them separately reseeds against the
    // *old* mood and pitches every voice from the wrong scale.
    ne_set_piece(engine_, seed_, mood_, x_, y_);
    started_at_ = ne_elapsed(engine_);

    // A token can name a moment as well as a piece. Getting there means
    // running the harmony forward without producing the audio, which takes
    // about two seconds for half an hour - see ne_skip.
    if (q.at_minutes > 0) {
        ne_skip(engine_, (double)q.at_minutes * 60.0);
    }
}

double App::elapsed() const {
    const double e = ne_elapsed(engine_) - started_at_;
    return e < 0.0 ? 0.0 : e;
}

Piece App::bookmark() const {
    Piece p = piece();
    p.at_minutes = (int)(elapsed() / 60.0);
    return p;
}

void App::new_piece() {
    apply_piece(random_piece(mood_, x_, y_));
    save_prefs();
}

bool App::load_token(const std::string& text) {
    Piece p;
    if (!parse_piece(text, &p)) return false;
    apply_piece(p);
    save_prefs();
    return true;
}

void App::restart() { apply_piece(piece()); }

// -------------------------------------------------------------------- liked

bool App::is_liked() const {
    const std::string t = token();
    return std::find(liked_.begin(), liked_.end(), t) != liked_.end();
}

void App::toggle_liked() {
    const std::string t = token();
    const auto at = std::find(liked_.begin(), liked_.end(), t);
    if (at != liked_.end()) {
        liked_.erase(at);
    } else {
        // Newest first: the thing just liked is the thing most likely to be
        // wanted back.
        liked_.insert(liked_.begin(), t);
        if (liked_.size() > kMaxLiked) liked_.resize(kMaxLiked);
    }
    save_prefs();
}

// --------------------------------------------------------------------- level

void App::set_volume(float v) {
    volume_ = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    ne_set_gain(engine_, volume_);
    save_prefs();
}

// --------------------------------------------------------------------- sleep

void App::set_sleep(int minutes) {
    sleep_choice_ = minutes < 0 ? 0 : minutes;
    ne_set_sleep(engine_, (double)sleep_choice_ * 60.0);
}

float App::sleep_remaining() const { return (float)ne_sleep_remaining(engine_); }

// ------------------------------------------------------------------- picture

void App::set_fps(int f) {
    fps_ = f < 10 ? 10 : (f > 240 ? 240 : f);
    save_prefs();
}

void App::set_render_scale(float s) {
    render_scale_ = s < 0.25f ? 0.25f : (s > 1.0f ? 1.0f : s);
    save_prefs();
}

// --------------------------------------------------------------------- prefs

std::string App::prefs_path() const {
    char* base = SDL_GetPrefPath("nulleigenvalue", "NullEigenvalue");
    if (!base) return std::string();
    std::string p(base);
    SDL_free(base);
    return p + "settings.txt";
}

void App::load_prefs() {
    const std::string path = prefs_path();
    if (path.empty()) return;
    std::ifstream f(path);
    if (!f) return;

    std::string line;
    Piece restored = piece();
    bool have_piece = false;
    while (std::getline(f, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);
        if (key == "piece") {
            Piece p;
            if (parse_piece(val, &p)) {
                restored = p;
                have_piece = true;
            }
        } else if (key == "volume") {
            volume_ = (float)atof(val.c_str());
            volume_ = volume_ < 0.0f ? 0.0f : (volume_ > 1.0f ? 1.0f : volume_);
            ne_set_gain(engine_, volume_);
        } else if (key == "fps") {
            const int f = atoi(val.c_str());
            if (f >= 10 && f <= 240) fps_ = f;
        } else if (key == "detail") {
            const float s = (float)atof(val.c_str());
            if (s >= 0.25f && s <= 1.0f) render_scale_ = s;
        } else if (key == "liked") {
            Piece p;
            // Stored as a token, and re-parsed rather than trusted: a settings
            // file edited by hand should not be able to name a mood this build
            // does not have.
            if (parse_piece(val, &p) && liked_.size() < kMaxLiked) {
                liked_.push_back(p.quantised().token());
            }
        }
    }
    // Come back to the piece this was left on, from silence.
    if (have_piece) apply_piece(restored);
}

void App::save_prefs() const {
    const std::string path = prefs_path();
    if (path.empty()) return;
    std::ofstream f(path, std::ios::trunc);
    if (!f) return;
    f << "piece=" << token() << "\n";
    f << "volume=" << volume_ << "\n";
    f << "fps=" << fps_ << "\n";
    f << "detail=" << render_scale_ << "\n";
    for (const std::string& t : liked_) f << "liked=" << t << "\n";
}

}  // namespace ne
