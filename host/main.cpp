// Null Eigenvalue - a generative drone instrument.
//
// One window over the synthesis engine. The engine owns the audio device and
// the render callback is the OS audio thread talking straight to it, so this
// loop can jank, stall or be dragged around by a window manager and the drone
// does not notice. What crosses between them is a handful of atomics.
//
//   null_eigenvalue                                   just run
//   null_eigenvalue --shot 90 out.bmp --size 1920 1080
//
// The shot mode exists because the picture has to be checkable without a
// display: with SDL_VIDEODRIVER=offscreen it runs anywhere, which is how the
// composition and the chrome's scaling get verified in a container.

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "app.h"
#include "hud.h"
#include "nebula.h"
#include "nulleig.h"
#include "palette.h"
#include "panel.h"
#include "render.h"
#include "text.h"
#include "updater.h"

namespace {

#ifndef NE_VERSION
#define NE_VERSION ""
#endif

// A drone's picture does not need a gaming monitor's frame rate, and the cost
// of this composition is almost entirely blended pixels. Thirty is where the
// orbits still read as motion rather than as steps.
constexpr double kTargetFps = 30.0;

// Rasterise the field at three quarters of the window's pixels. Everything in
// it is a soft blob with no edge, so there is no detail to lose, and the cost
// scales with the square.
constexpr float kRenderScale = 0.75f;

struct Options {
    int shot_frames = 0;
    const char* shot_path = nullptr;
    int mood = -1;
    float x = 0.5f, y = 0.45f;
    int width = 1100, height = 720;
    bool show_panel = false;
};

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shot") == 0 && i + 2 < argc) {
            o.shot_frames = std::atoi(argv[i + 1]);
            o.shot_path = argv[i + 2];
            i += 2;
        } else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            o.width = std::atoi(argv[i + 1]);
            o.height = std::atoi(argv[i + 2]);
            i += 2;
        } else if (std::strcmp(argv[i], "--panel") == 0) {
            o.show_panel = true;
        } else if (std::strcmp(argv[i], "--mood") == 0 && i + 1 < argc) {
            o.mood = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--x") == 0 && i + 1 < argc) {
            o.x = (float)std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--y") == 0 && i + 1 < argc) {
            o.y = (float)std::atof(argv[++i]);
        }
    }
    return o;
}

// One number, so the transport, the dots and the lettering all grow together
// with the window and nothing has to be laid out by hand. The window is a
// bigger sheet of the same paper, not an excuse to lay it out again - past
// about one and a half the chrome starts competing with the picture.
//
// The display scale is not optional and its absence is a real bug, not a
// nicety. The layout numbers were designed against Flutter's *logical* pixels;
// everything here is in physical ones. On a 150% display that made the whole
// HUD two thirds of the size it was designed to be, and the bigger the screen
// the worse it looked - which reads exactly like "the UI does not scale".
float scale_for(SDL_Window* window, int w, int h) {
    float dpi = SDL_GetWindowDisplayScale(window);
    if (!(dpi > 0.1f)) dpi = 1.0f;  // no display to ask, e.g. offscreen
    const float shortest = (float)(w < h ? w : h) / dpi;
    const float s = shortest / 620.0f;
    return (s < 1.0f ? 1.0f : (s > 1.5f ? 1.5f : s)) * dpi;
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parse(argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Null Eigenvalue", opt.width, opt.height,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 1;
    }

    // Built before the first frame: the wavetables cost a couple of hundred
    // milliseconds and there is no point paying that mid-animation.
    ne_engine* engine = ne_create(48000);
    if (!engine) {
        std::fprintf(stderr, "the synthesis engine did not load\n");
        return 1;
    }
    // A device that will not open is survivable - it is exactly what happens
    // in a container - and it must not be the reason the window never appears.
    if (ne_start(engine) != 0) {
        std::fprintf(stderr, "audio device did not open (silent run)\n");
    }

    ne::App app(engine);
    app.load_prefs();
    ne::NebulaState state;
    state.render_scale = kRenderScale;

    ne::Renderer draw;
    if (!draw.init(renderer)) {
        std::fprintf(stderr, "renderer init failed: %s\n", SDL_GetError());
        return 1;
    }
    ne::TextRenderer text;
    text.init(renderer);
    ne::Hud hud;
    ne::HudModel hm;
    ne::Panel panel;
    ne::PanelModel pm;
    ne::Updater updater(NE_VERSION);

    // ---------------------------------------------------------------- headless
    if (opt.shot_frames > 0) {
        if (opt.mood >= 0) app.set_mood(opt.mood);
        app.set_field(opt.x, opt.y, false);
        app.set_playing(true);

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);

        // Drive the synthesis by hand. Without a device nothing ever calls
        // ne_render, so the visuals would sit frozen at their idle state and
        // the shot would show the one thing that is not a reading of the
        // engine.
        constexpr int kAudioPerFrame = (int)(48000.0 / kTargetFps);
        std::vector<float> scratch((size_t)kAudioPerFrame * 2, 0.0f);
        const float dt = 1.0f / (float)kTargetFps;

        for (int i = 0; i < opt.shot_frames; ++i) {
            ne_render(engine, scratch.data(), kAudioPerFrame);
            ne_vis v{};
            ne_get_vis(engine, &v);
            app.tick(dt);
            state.palette = app.palette();
            state.advance(dt, v);
            state.idle_hint = 0.0f;
            draw.draw(state, w, h);

            hm.mood = app.mood();
            hm.playing = app.playing();
            hm.root_hz = v.root_hz;
            hm.instrument = app.instrument_name();
            hm.token = app.token();
            hm.elapsed = app.elapsed();
            hm.amount = 1.0f;
            if (opt.show_panel) {
                pm.amount = 1.0f;
                pm.sleep_choice_min = app.sleep_choice();
                pm.sleep_remaining = app.sleep_remaining();
                pm.volume = app.volume();
                pm.fps = app.fps();
                pm.render_scale = app.render_scale();
                pm.token = app.token();
                pm.liked = app.is_liked();
                pm.liked_list = app.liked();
                pm.updates_enabled = true;
                pm.update_status = "UP TO DATE";
                panel.build(pm, (float)w, (float)h, scale_for(window, w, h), text);
                panel.draw(pm, state.palette, draw, text);
            } else {
                hud.layout(hm, (float)w, (float)h, scale_for(window, w, h), text);
                hud.draw(hm, state.palette, draw, text);
            }

            SDL_RenderPresent(renderer);
        }
        SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
        if (shot) {
            if (!SDL_SaveBMP(shot, opt.shot_path)) {
                std::fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
            } else {
                std::printf("wrote %s (%dx%d, %d frames)\n", opt.shot_path, w, h,
                            opt.shot_frames);
            }
            SDL_DestroySurface(shot);
        }
        text.shutdown();
        draw.shutdown();
        ne_destroy(engine);
        SDL_Quit();
        return 0;
    }

    // ------------------------------------------------------------------- loop
    bool running = true;
    bool dragging = false;
    bool fullscreen = false;
    Uint64 last = SDL_GetTicksNS();

    // The chrome behaves the way it does over a video: a mouse that moves
    // raises it, and four seconds of stillness takes it away again - the
    // pointer with it, so a drone left running overnight is the picture and
    // nothing else.
    float since_start = 0;
    bool checked = false;

    bool hud_up = true;
    bool panel_up = false;
    float panel_amt = 0;
    float hud_amt = 0;
    float hud_idle = 0;
    float last_mx = 0, last_my = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN: {
                    const SDL_Keycode k = e.key.key;
                    // Escape unwinds one layer at a time rather than always
                    // quitting: leaving full screen is what it means when
                    // there is a full screen to leave.
                    if (k == SDLK_ESCAPE) {
                        if (panel_up) {
                            panel_up = false;
                        } else if (fullscreen) {
                            fullscreen = false;
                            SDL_SetWindowFullscreen(window, false);
                        } else {
                            running = false;
                        }
                    }
                    if (k == SDLK_F11 || k == SDLK_F) {
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window, fullscreen);
                    }
                    if (k == SDLK_S) panel_up = !panel_up;
                    if (k == SDLK_SPACE) app.toggle();
                    // The arrows are the field itself, not a menu: the same
                    // two axes the pointer drags through, in steps small
                    // enough that holding a key reads as a slow sweep.
                    {
                        const float step = 0.035f;
                        float nx = app.field_x(), ny = app.field_y();
                        if (k == SDLK_LEFT) nx -= step;
                        if (k == SDLK_RIGHT) nx += step;
                        if (k == SDLK_UP) ny += step;
                        if (k == SDLK_DOWN) ny -= step;
                        if (nx != app.field_x() || ny != app.field_y()) {
                            app.set_field(nx, ny, false);
                            state.target = ne::Vec2{app.field_x(), 1.0f - app.field_y()};
                            state.add_trail(state.target);
                            app.save_prefs();
                        }
                    }
                    // Where every application that has a zoom puts its zoom,
                    // and this app has no zoom.
                    if (k == SDLK_MINUS || k == SDLK_KP_MINUS) app.nudge_volume(-0.04f);
                    if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) {
                        app.nudge_volume(0.04f);
                    }
                    if (k >= SDLK_1 && k <= SDLK_6) app.set_mood((int)(k - SDLK_1));

                    // The piece, as a thing you can keep rather than a thing
                    // that happens to be playing.
                    if (k == SDLK_N) app.new_piece();
                    if (k == SDLK_R) app.restart();
                    if (k == SDLK_L) app.toggle_liked();
                    if (k == SDLK_C) SDL_SetClipboardText(app.token().c_str());
                    // The moment, not just the piece. Pressing this after
                    // something good is the only way back to it: a piece runs
                    // for days without repeating and there is nothing to
                    // rewind, so the way back is to write down where you were.
                    if (k == SDLK_M) {
                        SDL_SetClipboardText(app.bookmark().token().c_str());
                    }
                    if (k == SDLK_V) {
                        char* clip = SDL_GetClipboardText();
                        if (clip) {
                            if (!app.load_token(clip)) {
                                std::fprintf(stderr, "clipboard holds no piece name\n");
                            }
                            SDL_free(clip);
                        }
                    }
                    hud_up = true;
                    hud_idle = 0;
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL:
                    hud_up = true;
                    hud_idle = 0;
                    if (panel_up) {
                        // While the panel is open the wheel belongs to it.
                        panel.scroll_by(-e.wheel.y * 48.0f);
                    } else {
                        // Sign only, not magnitude: a trackpad fling delivers
                        // deltas an order of magnitude larger than a wheel
                        // detent, and scaling by them makes a flick go from
                        // silence to full.
                        app.nudge_volume(e.wheel.y > 0 ? 0.04f : -0.04f);
                    }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    // Only a real move, so a window that merely gained focus
                    // does not raise the chrome on its own.
                    if (std::fabs(e.motion.x - last_mx) > 0.5f ||
                        std::fabs(e.motion.y - last_my) > 0.5f) {
                        hud_up = true;
                        hud_idle = 0;
                    }
                    last_mx = e.motion.x;
                    last_my = e.motion.y;
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                    // The chrome gets first refusal on a click; only what it
                    // does not want becomes a drag on the field.
                    hud_idle = 0;
                    hud_up = true;
                    if (panel_amt > 0.5f) {
                        const ne::PanelHit ph = panel.hit_test(e.button.x, e.button.y);
                        switch (ph.action) {
                            case ne::PanelAction::Sleep: app.set_sleep(ph.value); break;
                            case ne::PanelAction::Volume:
                                app.nudge_volume((float)ph.value * 0.04f);
                                break;
                            case ne::PanelAction::Fps: app.set_fps(ph.value); break;
                            case ne::PanelAction::RenderScale:
                                app.set_render_scale((float)ph.value / 100.0f);
                                break;
                            case ne::PanelAction::NewPiece: app.new_piece(); break;
                            case ne::PanelAction::RestartPiece: app.restart(); break;
                            case ne::PanelAction::LikePiece: app.toggle_liked(); break;
                            case ne::PanelAction::CopyPiece:
                                SDL_SetClipboardText(app.token().c_str());
                                break;
                            case ne::PanelAction::CopyMoment:
                                SDL_SetClipboardText(
                                    app.bookmark().token().c_str());
                                break;
                            case ne::PanelAction::PastePiece: {
                                char* clip = SDL_GetClipboardText();
                                if (clip) {
                                    app.load_token(clip);
                                    SDL_free(clip);
                                }
                                break;
                            }
                            case ne::PanelAction::PlayLiked:
                                if (ph.value >= 0 &&
                                    ph.value < (int)app.liked().size()) {
                                    app.load_token(app.liked()[(size_t)ph.value]);
                                }
                                break;
                            case ne::PanelAction::UpdateCheck:
                                updater.check(true);
                                break;
                            case ne::PanelAction::UpdateAuto:
                                updater.set_automatic(!updater.automatic());
                                break;
                            case ne::PanelAction::UpdateInstall:
                                updater.install();
                                break;
                            case ne::PanelAction::Close: panel_up = false; break;
                            default: break;
                        }
                        break;
                    }
                    const ne::HudResult r =
                        hud_amt > 0.5f ? hud.hit_test(e.button.x, e.button.y)
                                       : ne::HudResult{};
                    if (r.hit == ne::HudHit::Transport) {
                        app.toggle();
                    } else if (r.hit == ne::HudHit::Mood) {
                        app.set_mood(r.mood);
                    } else if (r.hit == ne::HudHit::None) {
                        dragging = true;
                        // While silent the whole window is the play button.
                        if (!app.playing()) app.set_playing(true);
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (dragging) {
                        dragging = false;
                        // Park the field where the pointer left it, without
                        // the touch flag: the engine stops hearing excitation
                        // but keeps the position.
                        app.set_field(app.field_x(), app.field_y(), false);
                        app.save_prefs();
                    }
                    break;
                default:
                    break;
            }
        }

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (w <= 0 || h <= 0) continue;

        const Uint64 now = SDL_GetTicksNS();
        float dt = (float)((double)(now - last) / 1e9);
        last = now;
        // Clamped: a window drag or a mode change hands us one enormous frame,
        // and an unclamped dt there throws the spring across the screen.
        if (dt > 0.05f) dt = 0.05f;
        if (dt <= 0.0f) dt = 1.0f / (float)kTargetFps;

        if (dragging) {
            float mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            const ne::Vec2 t{std::min(1.0f, std::max(0.0f, mx / (float)w)),
                             std::min(1.0f, std::max(0.0f, my / (float)h))};
            state.target = t;
            state.add_trail(t);
            // y is inverted: the engine's density axis runs upward.
            app.set_field(t.x, 1.0f - t.y, true);
        }

        app.tick(dt);
        state.palette = app.palette();
        state.render_scale = app.render_scale();

        since_start += dt;
        if (!checked && since_start > 6.0f) {
            checked = true;
            updater.check();
        }

        ne_vis v{};
        ne_get_vis(engine, &v);
        state.advance(dt, v);

        // Four seconds of stillness, but only while something is playing: a
        // stopped app that hides its own transport has no way back.
        hud_idle += dt;
        if (hud_idle > 4.0f && app.playing()) hud_up = false;
        hud_amt += ((hud_up ? 1.0f : 0.0f) - hud_amt) * (1.0f - std::exp(-dt / 0.28f));
        // The centre glyph and the chrome cross-fade rather than swapping, so
        // raising the HUD while stopped does not make the invitation vanish on
        // a frame boundary.
        state.idle_hint = (app.playing() ? 0.0f : 1.0f) * (1.0f - hud_amt);

        draw.draw(state, w, h);

        if (hud_amt > 0.01f && panel_amt < 0.5f) {
            hm.mood = app.mood();
            hm.playing = app.playing();
            hm.root_hz = v.root_hz;
            hm.instrument = app.instrument_name();
            hm.token = app.token();
            hm.elapsed = app.elapsed();
            hm.amount = hud_amt;
            hud.layout(hm, (float)w, (float)h, scale_for(window, w, h), text);
            hud.draw(hm, state.palette, draw, text);
        }
        panel_amt += ((panel_up ? 1.0f : 0.0f) - panel_amt) *
                     (1.0f - std::exp(-dt / 0.18f));
        if (panel_amt > 0.01f) {
            pm.amount = panel_amt;
            pm.sleep_choice_min = app.sleep_choice();
            pm.sleep_remaining = app.sleep_remaining();
            pm.volume = app.volume();
            pm.fps = app.fps();
            pm.render_scale = app.render_scale();
            pm.token = app.token();
            pm.liked = app.is_liked();
            pm.liked_list = app.liked();
            pm.updates_enabled = updater.enabled();
            pm.update_auto = updater.automatic();
            pm.update_actionable = updater.stage() == ne::UpdateStage::Available;
            pm.update_status = updater.status_line();
            panel.build(pm, (float)w, (float)h, scale_for(window, w, h), text);
            panel.draw(pm, state.palette, draw, text);
        }

        // Nothing to point at while the chrome is away, so the pointer goes.
        if (hud_amt < 0.05f && !panel_up) {
            SDL_HideCursor();
        } else {
            SDL_ShowCursor();
        }

        SDL_RenderPresent(renderer);

        const Uint64 spent = SDL_GetTicksNS() - now;
        const Uint64 budget = (Uint64)(1e9 / (double)app.fps());
        if (spent < budget) SDL_DelayNS(budget - spent);
    }

    text.shutdown();
    draw.shutdown();
    ne_destroy(engine);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
