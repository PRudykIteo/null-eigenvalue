// The one panel: everything that needs explaining, behind the gear.
//
// Built as a list of laid-out items rather than drawn straight to the screen,
// so the geometry exists once and both the drawing and the hit testing read
// the same numbers. A panel whose layout is written twice is a panel whose
// buttons stop lining up with themselves the first time a row is added.
#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "palette.h"
#include "render.h"
#include "text.h"

namespace ne {

enum class PanelAction {
    None,
    Sleep,        // value = minutes, 0 = off
    Volume,       // value = -1 down, +1 up
    Fps,          // value = the new cap
    RenderScale,  // value = per cent
    NewPiece,
    RestartPiece,
    CopyPiece,
    CopyMoment,
    PastePiece,
    LikePiece,
    PlayLiked,    // value = index into liked
    UpdateCheck,
    UpdateAuto,
    UpdateInstall,
    Close,
};

struct PanelHit {
    PanelAction action = PanelAction::None;
    int value = 0;
};

struct PanelModel {
    float amount = 0;  // 0 hidden, 1 fully up

    int sleep_choice_min = 0;      // what is armed, 0 = off
    float sleep_remaining = -1;    // seconds, negative when disarmed

    float volume = 0.7f;
    int fps = 30;
    float render_scale = 0.75f;

    std::string token;
    bool liked = false;
    std::vector<std::string> liked_list;

    bool updates_enabled = false;
    bool update_auto = true;
    bool update_actionable = false;   // a newer version is there to install
    std::string update_status;
};

class Panel {
 public:
    void build(const PanelModel& m, float w, float h, float scale, TextRenderer& text);
    void draw(const PanelModel& m, const MoodPalette& p, Renderer& r, TextRenderer& text);

    PanelHit hit_test(float x, float y) const;

    // The panel is taller than a short window, so it scrolls. Clamped in
    // build(), which is the only place that knows how tall it turned out.
    void scroll_by(float dy);

 private:
    enum class Kind { Title, Heading, Row, Note, Divider, KeyRow };

    struct Item {
        Kind kind = Kind::Row;
        SDL_FRect rect{0, 0, 0, 0};
        std::string text;
        std::string second;   // the meaning, for a key row
        bool selected = false;
        bool dim = false;
        PanelAction action = PanelAction::None;
        int value = 0;
    };

    std::vector<Item> items_;
    float scroll_ = 0;
    float content_h_ = 0;
    float view_h_ = 0;
    float scale_ = 1;
};

}  // namespace ne
