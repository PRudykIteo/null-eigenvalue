#include "panel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ne {
namespace {

// The sleep durations. Five, because a list long enough to need reading is a
// list you have to think about at the point of going to sleep.
const int kSleepMinutes[] = {15, 30, 45, 60, 90};

// Two knobs that decide what the picture costs, offered rather than guessed:
// a machine this runs on overnight is not necessarily the one it was tuned on.
const int kFpsOptions[] = {24, 30, 45, 60};
const int kScaleOptions[] = {50, 75, 100};

struct KeyBinding {
    const char* key;
    const char* meaning;
};

// A chromeless app that also hides its shortcuts is just a locked door.
const KeyBinding kKeys[] = {
    {"SPACE", "PLAY / PAUSE"}, {"1 - 6", "MOOD"},        {"ARROWS", "FIELD"},
    {"SCROLL", "VOLUME"},      {"- / =", "VOLUME"},      {"F", "FULL SCREEN"},
    {"S", "THIS PANEL"},       {"D", "DIAGNOSTICS"},     {"N", "NEW PIECE"},
    {"R", "RESTART PIECE"},    {"L", "LIKE THIS PIECE"}, {"C", "COPY ITS NAME"},
    {"M", "COPY THIS MOMENT"},
    {"V", "PASTE A PIECE"},    {"ESC", "CLOSE / WINDOW"},
};

std::string minutes_label(int m) {
    if (m == 0) return "OFF";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d MIN", m);
    return std::string(buf);
}

std::string countdown(float seconds) {
    if (seconds < 0) return std::string();
    const int total = (int)seconds;
    char buf[32];
    if (total >= 3600) {
        std::snprintf(buf, sizeof(buf), "SLEEP %d:%02d:%02d", total / 3600,
                      (total / 60) % 60, total % 60);
    } else {
        std::snprintf(buf, sizeof(buf), "SLEEP %d:%02d", total / 60, total % 60);
    }
    return std::string(buf);
}

}  // namespace

void Panel::scroll_by(float dy) {
    scroll_ += dy;
    const float max = std::max(0.0f, content_h_ - view_h_);
    scroll_ = std::min(max, std::max(0.0f, scroll_));
}

void Panel::build(const PanelModel& m, float w, float h, float scale,
                  TextRenderer& text) {
    (void)text;
    items_.clear();
    scale_ = scale;
    view_h_ = h;

    const float col_w = 230.0f * scale;
    const float gap = 56.0f * scale;
    const float total_w = col_w * 2.0f + gap;
    const float left_x = w * 0.5f - total_w * 0.5f;
    const float right_x = left_x + col_w + gap;

    const float row_h = 26.0f * scale;
    const float head_h = 30.0f * scale;
    const float note_h = 20.0f * scale;
    const float div_h = 34.0f * scale;

    // Laid out from a nominal top; the scroll offset is applied at the end, so
    // the content height is known before anything is positioned on screen.
    float ly = 0, ry = 0;

    auto put = [&](float& y, float x, Kind k, const std::string& t, float height,
                   PanelAction a = PanelAction::None, int v = 0,
                   bool sel = false) {
        Item it;
        it.kind = k;
        it.rect = SDL_FRect{x, y, col_w, height};
        it.text = t;
        it.action = a;
        it.value = v;
        it.selected = sel;
        items_.push_back(it);
        y += height;
        return (int)items_.size() - 1;
    };

    // ---- left column: sleep, volume, picture, updates ----------------------
    put(ly, left_x, Kind::Title, "SLEEP", head_h);
    for (int mins : kSleepMinutes) {
        put(ly, left_x, Kind::Row, minutes_label(mins), row_h, PanelAction::Sleep,
            mins, m.sleep_choice_min == mins);
    }
    put(ly, left_x, Kind::Row, minutes_label(0), row_h, PanelAction::Sleep, 0,
        m.sleep_choice_min == 0);
    if (m.sleep_remaining >= 0) {
        put(ly, left_x, Kind::Note, countdown(m.sleep_remaining), note_h);
    }

    put(ly, left_x, Kind::Divider, "", div_h);
    put(ly, left_x, Kind::Heading, "VOLUME", head_h);
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d%%", (int)std::lround(m.volume * 100.0f));
        put(ly, left_x, Kind::Row, std::string("-      ") + buf + "      +", row_h,
            PanelAction::Volume, 0);
    }

    put(ly, left_x, Kind::Divider, "", div_h);
    put(ly, left_x, Kind::Heading, "PICTURE", head_h);
    for (int f : kFpsOptions) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%d FPS", f);
        put(ly, left_x, Kind::Row, buf, row_h, PanelAction::Fps, f, m.fps == f);
    }
    for (int s : kScaleOptions) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%d%% DETAIL", s);
        const int cur = (int)std::lround(m.render_scale * 100.0f);
        put(ly, left_x, Kind::Row, buf, row_h, PanelAction::RenderScale, s, cur == s);
    }

    if (m.updates_enabled) {
        put(ly, left_x, Kind::Divider, "", div_h);
        put(ly, left_x, Kind::Heading, "UPDATES", head_h);
        put(ly, left_x, Kind::Row, m.update_auto ? "AUTOMATIC  ON" : "AUTOMATIC  OFF",
            row_h, PanelAction::UpdateAuto, 0, m.update_auto);
        put(ly, left_x, Kind::Row, "CHECK NOW", row_h, PanelAction::UpdateCheck, 0);
        if (m.update_actionable) {
            put(ly, left_x, Kind::Row, "INSTALL", row_h, PanelAction::UpdateInstall, 0,
                true);
        }
        if (!m.update_status.empty()) {
            put(ly, left_x, Kind::Note, m.update_status, note_h);
        }
    }

    // ---- right column: pieces, then the keys -------------------------------
    put(ry, right_x, Kind::Title, "PIECES", head_h);
    put(ry, right_x, Kind::Note, m.token.empty() ? "NE1-....-....-...." : m.token,
        note_h);
    put(ry, right_x, Kind::Row, "NEW", row_h, PanelAction::NewPiece, 0);
    put(ry, right_x, Kind::Row, "RESTART", row_h, PanelAction::RestartPiece, 0);
    put(ry, right_x, Kind::Row, "COPY", row_h, PanelAction::CopyPiece, 0);
    put(ry, right_x, Kind::Row, "COPY THIS MOMENT", row_h, PanelAction::CopyMoment, 0);
    put(ry, right_x, Kind::Row, "PASTE", row_h, PanelAction::PastePiece, 0);
    put(ry, right_x, Kind::Row, m.liked ? "LIKED" : "LIKE", row_h,
        PanelAction::LikePiece, 0, m.liked);

    if (!m.liked_list.empty()) {
        put(ry, right_x, Kind::Divider, "", div_h);
        put(ry, right_x, Kind::Heading, "LIKED", head_h);
        for (size_t i = 0; i < m.liked_list.size(); ++i) {
            put(ry, right_x, Kind::Row, m.liked_list[i], row_h, PanelAction::PlayLiked,
                (int)i, m.liked_list[i] == m.token);
        }
    }

    put(ry, right_x, Kind::Divider, "", div_h);
    put(ry, right_x, Kind::Heading, "KEYS", head_h);
    for (const KeyBinding& k : kKeys) {
        Item it;
        it.kind = Kind::KeyRow;
        it.rect = SDL_FRect{right_x, ry, col_w, row_h};
        it.text = k.key;
        it.second = k.meaning;
        items_.push_back(it);
        ry += row_h;
    }

    content_h_ = std::max(ly, ry);

    // Centred when it fits, scrolled when it does not - so an ordinary window
    // shows no sign that there could be anything to scroll.
    const float margin = 40.0f * scale;
    float top;
    if (content_h_ + margin * 2.0f <= h) {
        top = (h - content_h_) * 0.5f;
        scroll_ = 0;
    } else {
        const float max = content_h_ - (h - margin * 2.0f);
        scroll_ = std::min(max, std::max(0.0f, scroll_));
        top = margin - scroll_;
    }
    for (Item& it : items_) it.rect.y += top;
}

void Panel::draw(const PanelModel& m, const MoodPalette& p, Renderer& r,
                 TextRenderer& text) {
    const float a = m.amount;
    if (a <= 0.01f) return;

    // A scrim, so the panel is read against something rather than against
    // whatever the music happens to be doing behind it.
    r.rect(0, 0, 100000.0f, 100000.0f, Colour{0, 0, 0, 0.55f * a});

    for (const Item& it : items_) {
        // Off the top or bottom of the window: laid out, not drawn.
        if (it.rect.y + it.rect.h < 0 || it.rect.y > view_h_) continue;

        const float cx = it.rect.x + it.rect.w * 0.5f;
        switch (it.kind) {
            case Kind::Title:
                text.draw_centred(it.text, cx, it.rect.y + 12.0f * scale_,
                                  Weight::Light, 12.0f * scale_, 6.5f * scale_,
                                  Colour{1, 1, 1, 0.55f * a});
                break;
            case Kind::Heading:
                text.draw_centred(it.text, cx, it.rect.y + 12.0f * scale_,
                                  Weight::Light, 11.0f * scale_, 4.6f * scale_,
                                  Colour{1, 1, 1, 0.45f * a});
                break;
            case Kind::Note:
                text.draw_centred(it.text, cx, it.rect.y + 11.0f * scale_,
                                  Weight::Light, 10.0f * scale_, 2.4f * scale_,
                                  Colour{1, 1, 1, 0.28f * a});
                break;
            case Kind::Divider:
                r.rect(cx - 100.0f * scale_, it.rect.y + it.rect.h * 0.5f,
                       200.0f * scale_, 1.0f, Colour{1, 1, 1, 0.08f * a});
                break;
            case Kind::Row: {
                // Selected rows are in the mood's accent, the rest in the same
                // whisper-grey as everything else. The row is not a button
                // with a border; it is a word you can touch.
                const Colour c = it.selected ? p.accent.with_alpha(0.92f * a)
                                             : Colour{1, 1, 1, 0.40f * a};
                text.draw_centred(it.text, cx, it.rect.y + 11.0f * scale_,
                                  it.selected ? Weight::Regular : Weight::Light,
                                  10.0f * scale_, 2.4f * scale_, c);
                break;
            }
            case Kind::KeyRow: {
                // Two columns: the key hard against the middle, its meaning
                // just after it, so the eye can run down either side.
                const float mid = it.rect.x + it.rect.w * 0.42f;
                const float kw = text.measure(it.text, Weight::Regular, 10.0f * scale_,
                                              1.8f * scale_);
                text.draw(it.text, mid - kw, it.rect.y + 11.0f * scale_,
                          Weight::Regular, 10.0f * scale_, 1.8f * scale_,
                          Colour{1, 1, 1, 0.34f * a});
                text.draw(it.second, mid + 14.0f * scale_, it.rect.y + 11.0f * scale_,
                          Weight::Light, 10.0f * scale_, 1.8f * scale_,
                          Colour{1, 1, 1, 0.22f * a});
                break;
            }
        }
    }
}

PanelHit Panel::hit_test(float x, float y) const {
    for (const Item& it : items_) {
        if (it.action == PanelAction::None) continue;
        if (x < it.rect.x || x > it.rect.x + it.rect.w) continue;
        if (y < it.rect.y || y > it.rect.y + it.rect.h) continue;
        if (it.action == PanelAction::Volume) {
            // One row, two halves: left of centre is down, right is up.
            const float cx = it.rect.x + it.rect.w * 0.5f;
            return PanelHit{PanelAction::Volume, x < cx ? -1 : 1};
        }
        return PanelHit{it.action, it.value};
    }
    // Anywhere else closes it. The panel is a bedside switch, not a screen.
    return PanelHit{PanelAction::Close, 0};
}

}  // namespace ne
