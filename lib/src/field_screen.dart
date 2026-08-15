import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter/services.dart';
import 'package:nulleig/nulleig.dart';

import 'drone_controller.dart';
import 'hud.dart';
import 'nebula.dart';
import 'palette.dart';
import 'piece.dart';
import 'platform.dart';
import 'textures.dart';
import 'updater.dart';

/// The app. There is one screen and it is the instrument.
class FieldScreen extends StatefulWidget {
  const FieldScreen({
    super.key,
    required this.controller,
    required this.textures,
    required this.updater,
  });

  final DroneController controller;
  final Textures textures;
  final Updater updater;

  @override
  State<FieldScreen> createState() => _FieldScreenState();
}

class _FieldScreenState extends State<FieldScreen>
    with SingleTickerProviderStateMixin {
  final NebulaState _state = NebulaState();
  late final Ticker _ticker;

  Duration _lastTick = Duration.zero;
  double _idle = 1;

  /// Time banked since the last redraw, and the rate actually achieved.
  ///
  /// The ticker fires at the display's refresh rate, which on this kind of
  /// machine is anything up to 165 Hz. Nothing in this picture moves fast
  /// enough to need that - the spring settles over a third of a second and
  /// everything else is measured in tens of seconds - and every one of those
  /// frames is a dozen very large blended shapes. So the clock still runs at
  /// vsync and the picture is redrawn on a budget.
  double _sinceFrame = 0;
  double _fps = 0;

  // Hidden at launch. The painter already draws a large breathing ring with a
  // play glyph in the middle of the field while the app is silent, and the HUD
  // has a transport of its own - showing both at once put two play buttons on
  // screen, one of them apparently decorative.
  bool _hudVisible = false;
  double _hudAmt = 0;
  Timer? _hudTimer;

  DroneStatus _status = DroneStatus.unknown;
  double _statusClock = 0;
  bool _forceDiagnostics = false;

  // Callback throughput between status polls. `callbacks` alone cannot tell a
  // live device from one that served four thousand buffers and then silently
  // died with its state still saying "started" - the *rate* can.
  int _lastCallbacks = -1;
  double _cbPerSec = 0;

  // The sleep panel, and the last countdown second the HUD was rebuilt for.
  // The countdown lives in the engine; the UI only needs a repaint when the
  // displayed second changes, and only while something is showing it.
  bool _sleepVisible = false;
  int _sleepSecShown = -1;

  /// Pixels the finger has covered since the last frame, which is where the
  /// excitation the engine hears comes from. Accumulated here rather than
  /// measured per pointer event because pointer events arrive in bursts and
  /// dividing one delta by one inter-event time gives a number that swings by
  /// an order of magnitude between frames.
  double _movedPx = 0;
  bool _touching = false;

  /// Desktop only. Whether the window is filling the screen, mirrored here so
  /// that Escape knows whether it has something to leave.
  bool _fullscreen = false;

  /// Whether the level readout is currently showing. It appears when the
  /// volume moves and takes itself away again, so the resting HUD is the same
  /// four lines it has always been.
  bool _volumeHint = false;
  Timer? _volumeHintTimer;

  /// The token as the chrome last drew it.
  ///
  /// It changes with the field, which changes on every pointer move - so it
  /// cannot come through notifyListeners without rebuilding the tree at the
  /// frame rate for a string nobody is looking at. Checked here instead, and
  /// only while there is something on screen showing it.
  String _tokenShown = '';

  /// Shown for a moment after the token goes to the clipboard.
  bool _copied = false;
  Timer? _copiedTimer;

  @override
  void initState() {
    super.initState();
    final c = widget.controller;
    _state.target = Offset(c.fieldX, 1 - c.fieldY);
    _state.centre = _state.target;
    _state.palette = c.palette;
    _state.renderScale = c.renderScale;
    c.addListener(_onControllerChanged);
    widget.updater.addListener(_onControllerChanged);
    _ticker = createTicker(_onTick)..start();
    _restartHudTimer();
  }

  @override
  void dispose() {
    _hudTimer?.cancel();
    _volumeHintTimer?.cancel();
    _copiedTimer?.cancel();
    _ticker.dispose();
    widget.controller.removeListener(_onControllerChanged);
    widget.updater.removeListener(_onControllerChanged);
    _state.dispose();
    super.dispose();
  }

  void _onControllerChanged() => setState(() {});

  void _onTick(Duration elapsed) {
    // Clamped: coming back from a minimised window hands us one enormous
    // frame, and an unclamped dt there would throw the spring across the
    // screen.
    final tick =
        ((elapsed - _lastTick).inMicroseconds / 1e6).clamp(0.0, 0.05);
    _lastTick = elapsed;
    if (tick <= 0) return;

    final c = widget.controller;

    // Bank the time and redraw on a budget. The simulation is advanced with
    // the whole banked interval rather than per vsync, so the spring and the
    // trails move at the same speed whatever the rate is set to - they are
    // integrated once per drawn frame instead of once per refresh.
    _sinceFrame += tick;
    if (c.frameRate > 0 && _sinceFrame < 1.0 / c.frameRate) return;
    final dt = _sinceFrame;
    _sinceFrame = 0;
    // Smoothed, because it is a diagnostic and an instantaneous frame time
    // reads as a broken counter.
    _fps += 0.1 * (1.0 / dt - _fps);
    // Rebuilt for as long as a mood crossfade is running. tickBlend moves the
    // palette without notifying - correct for the picture, which re-reads it
    // here every frame, and wrong for the HUD, which only sees it at build
    // time. Without this the chrome keeps the *previous* mood's accent until
    // something unrelated rebuilds it, so tapping a dot renamed the mood while
    // leaving its colour behind.
    final blending = c.moodBlend < 1;
    c.tickBlend(dt);
    if (blending) setState(() {});
    c.syncFromEngine();
    _state.palette = c.palette;
    _state.renderScale = c.renderScale;

    // Redraw the countdown once a second, and the token when it changes -
    // both only when there is something on screen showing them.
    if (_hudVisible || _sleepVisible) {
      final sleepSec = c.sleepRemaining?.inSeconds ?? -1;
      final token = c.token;
      if (sleepSec != _sleepSecShown || token != _tokenShown) {
        _sleepSecShown = sleepSec;
        _tokenShown = token;
        setState(() {});
      }
    }

    final wantIdle = c.playing ? 0.0 : 1.0;
    _idle += (wantIdle - _idle) * (1 - math.exp(-dt / 0.45));
    // The two invitations cross-fade rather than swap, so raising the HUD
    // while stopped does not make the centre glyph vanish on a frame boundary.
    _hudAmt += ((_hudVisible ? 1.0 : 0.0) - _hudAmt) * (1 - math.exp(-dt / 0.28));
    _state.idleHint = _idle * (1 - _hudAmt);

    _statusClock += dt;
    if (_statusClock > 0.5) {
      final s = c.status();
      if (_lastCallbacks >= 0 && s.callbacks >= _lastCallbacks) {
        _cbPerSec = (s.callbacks - _lastCallbacks) / _statusClock;
      }
      _lastCallbacks = s.callbacks;
      _statusClock = 0;
      _status = s;
    }

    if (_touching) {
      final w = context.size?.width ?? 400;
      c.setField(
        _state.target.dx,
        1 - _state.target.dy,
        touching: true,
        speed: (_movedPx / w) / dt,
      );
    }
    _movedPx = 0;

    _state.advance(dt, c.vis());
  }

  // ------------------------------------------------------------- gestures

  void _setFromLocal(Offset local, Size size) {
    _state.target = Offset(
      (local.dx / size.width).clamp(0.0, 1.0),
      (local.dy / size.height).clamp(0.0, 1.0),
    );
    _state.addTrail(_state.target);
  }

  void _onPanStart(DragStartDetails d) {
    final size = context.size;
    if (size == null) return;
    _touching = true;
    _movedPx = 0;
    _setFromLocal(d.localPosition, size);
    _hideHud();
  }

  void _onPanUpdate(DragUpdateDetails d) {
    final size = context.size;
    if (size == null) return;
    _movedPx += d.delta.distance;
    _setFromLocal(d.localPosition, size);
  }

  void _onPanEnd(_) {
    _touching = false;
    widget.controller.endTouch();
    // Park the final position without the touch flag, so the engine stops
    // hearing excitation but keeps the field where the finger left it.
    widget.controller.setField(
      _state.target.dx,
      1 - _state.target.dy,
      touching: false,
    );
  }

  void _onTap() {
    final c = widget.controller;
    if (!c.playing) {
      // While silent, the whole screen is the play button. Nothing else it
      // could reasonably mean.
      c.setPlaying(true);
      _showHud();
    } else {
      if (_hudVisible) {
        _hideHud();
      } else {
        _showHud();
      }
    }
  }

  /// Desktop only. A mouse that moves over the picture raises the chrome, the
  /// way it does over a video, and four seconds of stillness takes it away
  /// again - the pointer with it, so that a drone left running overnight is
  /// the picture and nothing else.
  void _onHover(PointerHoverEvent event) {
    if (event.delta == Offset.zero) return;
    _showHud();
  }

  void _showHud() {
    if (!_hudVisible) setState(() => _hudVisible = true);
    _restartHudTimer();
  }

  void _hideHud() {
    _hudTimer?.cancel();
    if (_hudVisible) setState(() => _hudVisible = false);
  }

  void _restartHudTimer() {
    _hudTimer?.cancel();
    _hudTimer = Timer(const Duration(seconds: 4), () {
      if (mounted && widget.controller.playing) {
        setState(() => _hudVisible = false);
      }
    });
  }

  // ------------------------------------------------------------- keyboard

  /// The keyboard is the desktop's version of the lock-screen controls: a way
  /// to drive the instrument without aiming at anything. The bindings are
  /// listed under the sleep options behind the gear, because a chromeless app
  /// that also hides its shortcuts is just a locked door.
  KeyEventResult _onKey(FocusNode node, KeyEvent event) {
    if (event is KeyUpEvent) return KeyEventResult.ignored;
    final c = widget.controller;
    final key = event.logicalKey;

    // Escape unwinds one layer at a time: the panel, then the window.
    if (key == LogicalKeyboardKey.escape) {
      if (_sleepVisible) {
        _closeSleep();
      } else if (_fullscreen) {
        _toggleFullscreen();
      } else {
        return KeyEventResult.ignored;
      }
      return KeyEventResult.handled;
    }

    // While the panel is open the keyboard belongs to it. There is a field in
    // there that takes a pasted piece name, and every shortcut below is a bare
    // letter - without this, typing a token into it plays six other pieces on
    // the way. Escape above is the way out, which is where anyone would look
    // for one anyway.
    if (_sleepVisible) return KeyEventResult.ignored;

    if (key == LogicalKeyboardKey.space) {
      c.toggle();
      _showHud();
      return KeyEventResult.handled;
    }

    if (key == LogicalKeyboardKey.f11 || key == LogicalKeyboardKey.keyF) {
      _toggleFullscreen();
      return KeyEventResult.handled;
    }

    if (key == LogicalKeyboardKey.keyS) {
      setState(() => _sleepVisible = !_sleepVisible);
      _hudTimer?.cancel();
      return KeyEventResult.handled;
    }

    // Where every application that has a zoom puts its zoom, and this app has
    // no zoom. Both the main row and the numeric keypad.
    if (key == LogicalKeyboardKey.minus ||
        key == LogicalKeyboardKey.numpadSubtract) {
      _changeVolume(-0.04);
      return KeyEventResult.handled;
    }
    if (key == LogicalKeyboardKey.equal ||
        key == LogicalKeyboardKey.add ||
        key == LogicalKeyboardKey.numpadAdd) {
      _changeVolume(0.04);
      return KeyEventResult.handled;
    }

    if (key == LogicalKeyboardKey.keyD) {
      setState(() => _forceDiagnostics = !_forceDiagnostics);
      _showHud();
      return KeyEventResult.handled;
    }

    // The piece, as four verbs. Deliberately on the home row rather than
    // behind the panel: finding a piece worth keeping happens while listening,
    // and a keepsake you have to open a settings panel to save is one you
    // mostly do not save.
    if (key == LogicalKeyboardKey.keyN) {
      c.newPiece();
      _showHud();
      return KeyEventResult.handled;
    }
    if (key == LogicalKeyboardKey.keyR) {
      c.restartPiece();
      _showHud();
      return KeyEventResult.handled;
    }
    if (key == LogicalKeyboardKey.keyL) {
      c.toggleLike();
      _showHud();
      return KeyEventResult.handled;
    }
    if (key == LogicalKeyboardKey.keyC) {
      _copyToken();
      return KeyEventResult.handled;
    }

    // The six moods, in the order the dots are drawn.
    const digits = <LogicalKeyboardKey>[
      LogicalKeyboardKey.digit1,
      LogicalKeyboardKey.digit2,
      LogicalKeyboardKey.digit3,
      LogicalKeyboardKey.digit4,
      LogicalKeyboardKey.digit5,
      LogicalKeyboardKey.digit6,
    ];
    final mood = digits.indexOf(key);
    if (mood >= 0 && mood < neMoodCount) {
      c.setMood(mood);
      _showHud();
      return KeyEventResult.handled;
    }

    // The arrows are the field itself, not a menu: the same two axes the
    // pointer drags through, in steps small enough that holding a key reads as
    // a slow sweep rather than as a jump.
    const step = 0.035;
    Offset? nudge;
    if (key == LogicalKeyboardKey.arrowLeft) nudge = const Offset(-step, 0);
    if (key == LogicalKeyboardKey.arrowRight) nudge = const Offset(step, 0);
    if (key == LogicalKeyboardKey.arrowUp) nudge = const Offset(0, -step);
    if (key == LogicalKeyboardKey.arrowDown) nudge = const Offset(0, step);
    if (nudge != null) {
      _nudgeField(nudge);
      return KeyEventResult.handled;
    }

    return KeyEventResult.ignored;
  }

  void _nudgeField(Offset delta) {
    _state.target = Offset(
      (_state.target.dx + delta.dx).clamp(0.0, 1.0),
      (_state.target.dy + delta.dy).clamp(0.0, 1.0),
    );
    // A trail point, same as a drag leaves. Without it the composition slides
    // and nothing marks that it was asked to.
    _state.addTrail(_state.target);
    widget.controller.setField(
      _state.target.dx,
      1 - _state.target.dy,
      touching: false,
    );
  }

  // --------------------------------------------------------------- volume

  /// The wheel is the level. It is the one gesture a desktop has spare - the
  /// picture has nothing to scroll - and it is where every other player on the
  /// machine already puts it.
  void _onPointerSignal(PointerSignalEvent event) {
    if (event is! PointerScrollEvent) return;
    // While the panel is open the wheel belongs to the panel, which has a
    // level control of its own and, in a short window, more rows than fit.
    if (_sleepVisible) return;
    // Sign only, not magnitude: a trackpad's fling delivers deltas an order of
    // magnitude larger than a wheel's detent, and scaling by them made a flick
    // go from silence to full.
    _changeVolume(event.scrollDelta.dy > 0 ? -0.04 : 0.04);
  }

  void _changeVolume(double delta) {
    widget.controller.nudgeVolume(delta);
    _showVolumeHint();
  }

  void _showVolumeHint() {
    if (!_volumeHint) setState(() => _volumeHint = true);
    _showHud();
    _volumeHintTimer?.cancel();
    _volumeHintTimer = Timer(const Duration(milliseconds: 2200), () {
      if (mounted) setState(() => _volumeHint = false);
    });
  }

  // ---------------------------------------------------------------- pieces

  /// The pieces section, as the plain values hud.dart wants. Translating here
  /// rather than handing the panel a controller keeps the chrome knowing
  /// nothing about the engine, which is the same split the painter has.
  PiecesPanel _piecesPanel(DroneController c) {
    final current = c.token;
    return PiecesPanel(
      token: current,
      liked: c.currentIsLiked,
      entries: <LikedEntry>[
        for (final p in c.liked)
          LikedEntry(
            token: p.token,
            mood: MoodPalette.all[p.mood].name,
            playing: p.token == current,
          ),
      ],
      onCopy: _copyToken,
      onLike: c.toggleLike,
      onNew: c.newPiece,
      onLoad: c.loadToken,
      onPlay: (t) {
        final p = Piece.parse(t);
        if (p != null) c.playPiece(p);
      },
      onRemove: (t) {
        final p = Piece.parse(t);
        if (p != null) c.removeLiked(p);
      },
    );
  }

  /// Puts the name of what is playing on the clipboard, and says so - the
  /// clipboard is the one place a user cannot look to check whether something
  /// happened.
  void _copyToken() {
    final token = widget.controller.token;
    unawaited(Clipboard.setData(ClipboardData(text: token)));
    _showHud();
    setState(() => _copied = true);
    _copiedTimer?.cancel();
    _copiedTimer = Timer(const Duration(milliseconds: 1800), () {
      if (mounted) setState(() => _copied = false);
    });
  }

  Future<void> _toggleFullscreen() async {
    final now = await AppWindow.toggleFullscreen();
    if (mounted) setState(() => _fullscreen = now);
  }

  @override
  Widget build(BuildContext context) {
    final c = widget.controller;
    final palette = c.palette;
    final media = MediaQuery.of(context);

    // One number, so the transport, the dots and the lettering all grow
    // together with the window and nothing has to be redrawn by hand.
    final shortest = media.size.shortestSide;
    final scale = (shortest / 620).clamp(1.0, 1.5).toDouble();

    // Nothing to point at while the chrome is away, so the pointer goes too.
    final hideCursor = !_hudVisible && !_sleepVisible;

    // The HUD stands down while the panel is open. It was always dead under
    // there - the panel's scrim is opaque to hits, so a tap on the transport
    // closed the panel rather than pausing anything - and once the panel grew
    // a key legend on the desktop the two also collided outright.
    final chrome = _hudVisible && !_sleepVisible;

    final Widget screen = Scaffold(
      backgroundColor: palette.bg,
      body: Stack(
        fit: StackFit.expand,
        children: <Widget>[
          GestureDetector(
            behavior: HitTestBehavior.opaque,
            onTap: _onTap,
            onPanStart: _onPanStart,
            onPanUpdate: _onPanUpdate,
            onPanEnd: _onPanEnd,
            onPanCancel: () => _onPanEnd(null),
            child: RepaintBoundary(
              child: CustomPaint(
                painter: NebulaPainter(
                  state: _state,
                  textures: widget.textures,
                ),
                // Not isComplex: that asks the raster cache to consider
                // keeping the picture, and a picture that is different every
                // time it is drawn can only ever be cached and thrown away.
                willChange: true,
              ),
            ),
          ),

          // Wordmark. Visible with the HUD only - it is a title, not a status.
          Positioned(
            top: media.padding.top + 22 * scale,
            left: 0,
            right: 0,
            child: IgnorePointer(
              child: AnimatedOpacity(
                opacity: chrome ? 1 : 0,
                duration: const Duration(milliseconds: 550),
                curve: Curves.easeOut,
                // The name, and after it the version at half the name's
                // weight. A downloaded app has no store page to look at, so
                // "which one am I running" has to be answerable from the app
                // itself - but it is a footnote to the title, not a second
                // title, so it shares the line and stays quieter than the
                // frequency readout.
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  crossAxisAlignment: CrossAxisAlignment.baseline,
                  textBaseline: TextBaseline.alphabetic,
                  children: <Widget>[
                    Text(
                      'NULL EIGENVALUE',
                      style: TextStyle(
                        fontSize: 11 * scale,
                        letterSpacing: 6.5 * scale,
                        fontWeight: FontWeight.w300,
                        color: Colors.white.withValues(alpha: 0.34),
                      ),
                    ),
                    Text(
                      _versionLabel,
                      style: TextStyle(
                        fontSize: 10 * scale,
                        letterSpacing: 2.4 * scale,
                        fontWeight: FontWeight.w300,
                        color: Colors.white.withValues(alpha: 0.18),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),

          // The gear. Rides in and out with the HUD, so the resting picture
          // stays chromeless; opens the one setting the app has.
          Positioned(
            top: media.padding.top + 8,
            right: 10,
            child: AnimatedOpacity(
              opacity: chrome ? 1 : 0,
              duration: const Duration(milliseconds: 550),
              curve: Curves.easeOut,
              child: IgnorePointer(
                ignoring: !chrome,
                child: GearButton(
                  colour: palette.accent,
                  scale: scale,
                  onTap: () {
                    setState(() => _sleepVisible = true);
                    _hudTimer?.cancel();
                  },
                ),
              ),
            ),
          ),

          Align(
            alignment: Alignment.bottomCenter,
            child: Padding(
              padding: EdgeInsets.only(bottom: media.padding.bottom + 34 * scale),
              child: AnimatedOpacity(
                opacity: chrome ? 1 : 0,
                duration: const Duration(milliseconds: 550),
                curve: Curves.easeOut,
                child: IgnorePointer(
                  ignoring: !chrome,
                  child: Hud(
                    palette: palette,
                    mood: c.mood,
                    playing: c.playing,
                    rootHz: _state.vis.rootHz,
                    scale: scale,
                    sleepLabel: _sleepLabel(c),
                    updateLabel: _updateLabel(),
                    onUpdateTap: _onUpdateTap,
                    volumeLabel: _copied
                        ? 'COPIED'
                        : _volumeHint
                            ? 'VOLUME ${(c.volume * 100).round()}%'
                            : null,
                    token: c.token,
                    liked: c.currentIsLiked,
                    onTokenTap: _copyToken,
                    onLike: () {
                      c.toggleLike();
                      _restartHudTimer();
                    },
                    diagnostics: _diagnostics(c),
                    onDiagnosticsTap: () {
                      setState(() => _forceDiagnostics = !_forceDiagnostics);
                      _restartHudTimer();
                    },
                    onMood: (m) {
                      c.setMood(m);
                      _restartHudTimer();
                    },
                    onToggle: () {
                      c.toggle();
                      _restartHudTimer();
                    },
                  ),
                ),
              ),
            ),
          ),

          // Sleep panel. A scrim and five words; tapping anywhere else is
          // "close". It deliberately does not pause, restyle or otherwise
          // comment on the music - it is a bedside switch, not a screen.
          Positioned.fill(
            child: IgnorePointer(
              ignoring: !_sleepVisible,
              child: AnimatedOpacity(
                opacity: _sleepVisible ? 1 : 0,
                duration: const Duration(milliseconds: 320),
                curve: Curves.easeOut,
                child: GestureDetector(
                  behavior: HitTestBehavior.opaque,
                  onTap: _closeSleep,
                  child: Container(
                    color: Colors.black.withValues(alpha: 0.55),
                    child: Center(
                      child: SettingsPanel(
                        accent: palette.accent,
                        remaining: c.sleepRemaining,
                        choice: c.sleepChoice,
                        scale: scale,
                        showKeys: true,
                        volume: c.volume,
                        onVolume: c.setVolume,
                        pieces: _piecesPanel(c),
                        picture: PicturePanel(
                          frameRate: c.frameRate,
                          renderScale: c.renderScale,
                          rates: DroneController.frameRates,
                          onFrameRate: c.setFrameRate,
                          onRenderScale: c.setRenderScale,
                        ),
                        // Only where there is a version to compare. A build
                        // CI did not cut would be claiming to be up to date
                        // on no evidence at all.
                        updates: widget.updater.enabled
                            ? UpdatePanel(
                                auto: widget.updater.auto,
                                busy: widget.updater.busy,
                                status: _updateStatus(),
                                onAuto: (v) =>
                                    unawaited(widget.updater.setAuto(v)),
                                onCheck: () => unawaited(
                                    widget.updater.check(force: true)),
                              )
                            : null,
                        onPick: _pickSleep,
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );

    // In this order: the pointer layer has to be inside the focus layer, or a
    // click on the picture would move focus off the node that owns the
    // keyboard and the shortcuts would go dead after the first drag.
    return Focus(
      autofocus: true,
      onKeyEvent: _onKey,
      child: MouseRegion(
        cursor: hideCursor ? SystemMouseCursors.none : MouseCursor.defer,
        onHover: _onHover,
        // Outside the MouseRegion's child rather than inside the Scaffold, so
        // the wheel works over the whole window - including over the HUD,
        // where a scroll that did nothing would read as the control being
        // stuck rather than as the HUD not being a scrollable thing.
        child: Listener(
          onPointerSignal: _onPointerSignal,
          child: screen,
        ),
      ),
    );
  }

  void _closeSleep() {
    setState(() => _sleepVisible = false);
    _restartHudTimer();
  }

  void _pickSleep(Duration? d) {
    widget.controller.setSleep(d);
    _closeSleep();
  }

  /// "SLEEP 27:41" under the frequency while a timer runs, or null.
  String? _sleepLabel(DroneController c) {
    final rem = c.sleepRemaining;
    if (rem == null) return null;
    final h = rem.inHours;
    final m = rem.inMinutes % 60;
    final s = rem.inSeconds % 60;
    final mm = m.toString().padLeft(2, '0');
    final ss = s.toString().padLeft(2, '0');
    return h > 0 ? 'SLEEP $h:$mm:$ss' : 'SLEEP $m:$ss';
  }

  /// What to draw after the wordmark. CI bakes NE_VERSION into release builds
  /// because they are downloaded rather than installed from a store, and there
  /// is no store page to go and read.
  String get _versionLabel => buildVersion.isEmpty ? '  DEV' : '  $buildVersion';

  /// One line about the newer version, in the same whisper as everything else
  /// down there. Null - which is almost always - means the app says nothing
  /// about updates at all.
  ///
  /// Only the states that are about a newer version actually existing appear
  /// here. "Checking", "up to date" and "could not check" are answers to a
  /// question asked in the panel and they belong in the panel: putting them on
  /// the picture would mean the app interrupts itself to say nothing happened.
  String? _updateLabel() {
    final u = widget.updater;
    switch (u.stage) {
      case UpdateStage.idle:
      case UpdateStage.checking:
      case UpdateStage.upToDate:
      case UpdateStage.checkFailed:
        return null;
      case UpdateStage.available:
        return 'UPDATE ${u.latest}';
      case UpdateStage.downloading:
        return 'UPDATING ${(u.progress * 100).round()}%';
      case UpdateStage.ready:
        return u.handoff ?? 'RESTARTING';
      case UpdateStage.failed:
        return 'UPDATE FAILED';
    }
  }

  /// The line under the two update rows in the panel, where every state has
  /// something to say.
  String _updateStatus() {
    final u = widget.updater;
    switch (u.stage) {
      case UpdateStage.idle:
        return u.auto ? 'NOT CHECKED YET' : 'AUTOMATIC IS OFF';
      case UpdateStage.checking:
        return 'CHECKING';
      case UpdateStage.upToDate:
        return 'UP TO DATE';
      case UpdateStage.checkFailed:
        return 'COULD NOT REACH GITHUB';
      case UpdateStage.available:
        return '${u.latest} IS AVAILABLE';
      case UpdateStage.downloading:
        return 'DOWNLOADING ${(u.progress * 100).round()}%';
      case UpdateStage.ready:
        return u.handoff ?? 'RESTARTING';
      case UpdateStage.failed:
        return 'DOWNLOAD FAILED';
    }
  }

  void _onUpdateTap() {
    final u = widget.updater;
    if (u.stage != UpdateStage.available) return;
    unawaited(u.install());
    _restartHudTimer();
  }

  /// The lines to show under the readout, or null to show nothing.
  ///
  /// It appears on its own when the audio is demonstrably not working, and on
  /// demand via the D key. The states it has to tell apart: a device that
  /// never opened, a device that opened but is never asked for audio, and a
  /// synthesizer that is being asked and is returning silence. Hence the
  /// second line - the engine's own play flag, gate and output level split
  /// "silence is ours" from "silence is downstream" - and `load`, which is the
  /// share of each audio buffer's worth of time the synthesis actually spends.
  String? _diagnostics(DroneController c) {
    final wanted = _forceDiagnostics ||
        !c.deviceOk ||
        (c.playing && _status.callbacks == 0) ||
        (c.playing && _status.elapsed > 5 && _state.vis.level < 0.0005);
    if (!wanted) return null;

    final v = _state.vis;
    final second = <String>[
      'play${c.enginePlaying ? 1 : 0}',
      'gate${v.gate.toStringAsFixed(2)}',
      'lvl${v.level.toStringAsFixed(3)}',
      'cb/s${_cbPerSec.round()}',
      'dsp${(_status.load * 100).toStringAsFixed(1)}%',
      'ui${_fps.round()}fps',
    ].join(' ');
    return '${_status.line}\n$second';
  }
}

