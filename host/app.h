// The app's durable state, and the one thing that is not the engine's: how far
// a mood change has got.
//
// Deliberately separate from the window and from the renderer. This knows
// about the engine and about what the user has chosen; it knows nothing about
// how the field is drawn.
#pragma once

#include <string>
#include <vector>

#include "nulleig.h"
#include "palette.h"
#include "piece.h"

namespace ne {

class App {
 public:
    explicit App(ne_engine* engine);

    void set_mood(int m);
    void toggle();
    void set_playing(bool p);
    void set_field(float x, float y, bool touching, float speed = 0);

    // Called from the frame clock.
    void tick(float dt);

    int mood() const { return mood_; }
    bool playing() const { return playing_; }
    float field_x() const { return x_; }
    float field_y() const { return y_; }

    // ------------------------------------------------------------- pieces

    // What is playing, as a name. Built live from the seed, the mood and where
    // the field is now - so dragging the field renames the piece, which is
    // honest: it is a different piece.
    Piece piece() const;
    std::string token() const { return piece().token(); }

    // A piece nobody has heard. Restarts the engine's streams from silence.
    void new_piece();

    // Plays what a token names. False when the string does not contain one,
    // in which case nothing changes.
    bool load_token(const std::string& text);

    // The same piece from the beginning, which is the only way to hear the
    // first minute again.
    void restart();

    // ------------------------------------------------------------- liked

    const std::vector<std::string>& liked() const { return liked_; }
    bool is_liked() const;
    void toggle_liked();

    // ------------------------------------------------------------- level

    float volume() const { return volume_; }
    void set_volume(float v);
    void nudge_volume(float delta) { set_volume(volume_ + delta); }

    // ------------------------------------------------------------- sleep

    // Minutes, or 0 to disarm. The countdown itself lives on the audio thread
    // against frames rendered, so it is the one clock that keeps time whatever
    // this loop is doing.
    void set_sleep(int minutes);
    int sleep_choice() const { return sleep_choice_; }
    float sleep_remaining() const;

    // ------------------------------------------------------------- picture

    int fps() const { return fps_; }
    void set_fps(int f);
    float render_scale() const { return render_scale_; }
    void set_render_scale(float s);

    // Preferences live next to the app's other per-user state. Both are
    // best-effort: an app that will not start because it could not read a
    // settings file is worse than one that starts on its defaults.
    void load_prefs();
    void save_prefs() const;

    // The palette as it should be drawn right now - mid-crossfade if a mood
    // was just changed. The engine runs its own, far slower migration in the
    // audio; this is only the colour.
    MoodPalette palette() const;

 private:
    ne_engine* engine_ = nullptr;
    int mood_ = 1;
    int prev_mood_ = 1;
    // 1 means settled. A mood change resets it to 0 and it walks back up.
    float blend_ = 1;
    bool playing_ = false;
    float x_ = 0.5f, y_ = 0.45f;

    uint32_t seed_ = 0x4E756C6Cu;
    std::vector<std::string> liked_;

    float volume_ = 0.7f;
    int sleep_choice_ = 0;
    int fps_ = 30;
    float render_scale_ = 0.75f;

    void apply_piece(const Piece& p);
    std::string prefs_path() const;
};

}  // namespace ne
