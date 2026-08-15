import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:nulleig/nulleig.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'palette.dart';
import 'piece.dart';

/// Owns the engine and the app's only piece of durable state.
///
/// The split with the UI is deliberate: this class knows nothing about how the
/// field is drawn and the painter knows nothing about FFI. What crosses
/// between them is four numbers and a mood.
class DroneController extends ChangeNotifier {
  DroneController(this.engine);

  final DroneEngine engine;

  int _mood = 1;
  double _x = 0.5;
  double _y = 0.45;
  bool _playing = false;
  bool _deviceOk = false;

  /// The seed the piece currently playing was started from.
  int _seed = 0;

  /// Tokens of the pieces worth coming back to, oldest first.
  final List<Piece> _liked = <Piece>[];

  /// Master gain, 0..1.
  ///
  /// Not quite 1 by default: the synthesizer is mastered to leave a little
  /// headroom, and starting at unity would mean the only direction the control
  /// goes is down. This is the app's own level, underneath whatever the system
  /// mixer says - which is the point of having it on a desktop, where the drone
  /// is one voice among a dozen other things making noise.
  double _volume = 0.92;

  /// How often the picture is redrawn, in frames per second. 0 means "as often
  /// as the display asks".
  ///
  /// Capped by default, and 30 rather than 60. This is a drone: nothing on
  /// screen moves faster than a spring settling over a third of a second, and
  /// the composition is a dozen very large blended shapes - so the frame rate
  /// is a straight multiplier on the most expensive thing the app does, buying
  /// nothing above 30 that anyone has ever been able to see. On a 144 Hz
  /// display this alone is most of the app's cost.
  int _frameRate = 30;

  /// The fraction of the window's pixels the field is rasterised at before
  /// being scaled back up, 0.5..1. Costs scale with the square of this.
  double _renderScale = 1;

  // Where the mood transition is up to, for the palette crossfade. The engine
  // does its own, much slower migration in the audio; this is only the colour.
  int _prevMood = 1;
  double _moodBlend = 1;

  SharedPreferences? _prefs;

  int get mood => _mood;
  int get previousMood => _prevMood;
  double get moodBlend => _moodBlend;
  double get fieldX => _x;
  double get fieldY => _y;
  bool get playing => _playing;
  bool get deviceOk => _deviceOk;
  double get volume => _volume;
  int get frameRate => _frameRate;
  double get renderScale => _renderScale;

  // ------------------------------------------------------------------ pieces

  /// What is playing, as the four numbers that would produce it again.
  ///
  /// Read live rather than stored, because three of the four are the
  /// instrument's own controls: moving the field or changing the mood makes
  /// this a different piece, and the readout should say so while it happens.
  ///
  /// One honest caveat, and it is worth knowing about. This is what you would
  /// hear if you *started* from these settings, which after a mood change is
  /// not quite what is coming out of the speakers: a mood is walked into over
  /// a minute or so rather than cut to, deliberately, and a piece that walked
  /// into Halo is not the piece that began there. The token is a bookmark of
  /// settings, not a recording of a moment - which is the only thing it could
  /// be for something with no end.
  Piece get piece =>
      Piece(seed: _seed, mood: _mood, x: _x, y: _y).quantised;

  String get token => piece.token;

  /// The liked pieces, oldest first.
  List<Piece> get liked => List<Piece>.unmodifiable(_liked);

  bool get currentIsLiked => _liked.contains(piece);

  /// Starts [p] from the beginning: its seed, its mood and its field, applied
  /// together so the engine cannot pick them up one at a time.
  void playPiece(Piece p) {
    final q = p.quantised;
    _seed = q.seed;
    if (q.mood != _mood) {
      _prevMood = _mood;
      _moodBlend = 0;
      _mood = q.mood;
    }
    _x = q.x;
    _y = q.y;
    engine.setPiece(seed: q.seed, mood: q.mood, x: q.x, y: q.y);
    _save();
    notifyListeners();
  }

  /// A piece nobody has heard, in the instrument currently set up.
  void newPiece() =>
      playPiece(Piece.random(mood: _mood, x: _x, y: _y));

  /// This piece again from the top - the same music, not merely the same
  /// settings.
  void restartPiece() => playPiece(piece);

  /// Loads a token. Returns false if it is not one, so the field that took it
  /// can say so rather than silently doing nothing.
  bool loadToken(String text) {
    final p = Piece.parse(text);
    if (p == null) return false;
    playPiece(p);
    return true;
  }

  void toggleLike() {
    final p = piece;
    if (!_liked.remove(p)) {
      _liked.add(p);
    }
    _saveLiked();
    notifyListeners();
  }

  void removeLiked(Piece p) {
    if (!_liked.remove(p)) return;
    _saveLiked();
    notifyListeners();
  }

  void _saveLiked() {
    _prefs?.setStringList(
        'liked', _liked.map((p) => p.token).toList(growable: false));
  }

  MoodPalette get palette => MoodPalette.lerp(
        MoodPalette.all[_prevMood],
        MoodPalette.all[_mood],
        _ease(_moodBlend),
      );

  String get moodName => MoodPalette.all[_mood].name;

  /// Called by whatever is driving the frame clock.
  void tickBlend(double dt) {
    if (_moodBlend < 1) {
      _moodBlend = math.min(1, _moodBlend + dt / 2.2);
    }
  }

  Future<void> restore() async {
    int? storedSeed;
    try {
      _prefs = await SharedPreferences.getInstance();
      _mood = (_prefs?.getInt('mood') ?? 1).clamp(0, neMoodCount - 1);
      _prevMood = _mood;
      _x = (_prefs?.getDouble('x') ?? 0.5).clamp(0.0, 1.0);
      _y = (_prefs?.getDouble('y') ?? 0.45).clamp(0.0, 1.0);
      // Floored well above zero. A drone that comes back silent because the
      // level was left at nothing last week is indistinguishable from one that
      // is broken, and this app has no other evidence to offer.
      _volume = (_prefs?.getDouble('volume') ?? 0.92).clamp(0.05, 1.0);
      _frameRate = _clampFrameRate(_prefs?.getInt('frameRate') ?? 30);
      _renderScale = (_prefs?.getDouble('renderScale') ?? 1.0).clamp(0.5, 1.0);
      storedSeed = _prefs?.getInt('seed');

      // Tokens rather than raw fields, so the stored form is the shareable
      // form and a list written by a build with different moods is rejected
      // entry by entry instead of loading as something else.
      for (final t in _prefs?.getStringList('liked') ?? const <String>[]) {
        final p = Piece.parse(t);
        if (p != null && !_liked.contains(p)) _liked.add(p);
      }
    } catch (_) {
      // A machine that will not give us preferences is not a reason to refuse
      // to make a sound.
    }

    // The app comes back to the piece it was left on. A drone whose whole
    // point is that you can name what you are hearing and come back to it
    // cannot forget which one it was between launches - and there is always
    // an obvious way to move on, so resuming costs nothing.
    _seed = storedSeed == null
        ? Piece.random(mood: _mood, x: _x, y: _y).seed
        : storedSeed & 0xFFFFFFFF;

    engine.setPiece(seed: _seed, mood: _mood, x: _x, y: _y);
    engine.gain = _volume;
    _save();
    notifyListeners();
  }

  /// Opens the audio device. Separate from [restore] so a preferences failure
  /// cannot be the reason the app makes no sound.
  void startAudio() {
    _deviceOk = engine.startDevice();
    notifyListeners();
  }

  DroneStatus status() => engine.status();

  /// The engine's own idea of whether it is playing, read back through FFI
  /// rather than mirrored from [_playing]. If these two ever disagree, the
  /// set_playing store is not reaching the synthesizer, and that is worth a
  /// line on screen.
  bool get enginePlaying => engine.playing;

  void _save() {
    _prefs?.setInt('mood', _mood);
    _prefs?.setDouble('x', _x);
    _prefs?.setDouble('y', _y);
    _prefs?.setDouble('volume', _volume);
    _prefs?.setInt('frameRate', _frameRate);
    _prefs?.setDouble('renderScale', _renderScale);
    _prefs?.setInt('seed', _seed);
  }

  /// The offered rates, and nothing between them. 0 is "uncapped".
  static const List<int> frameRates = <int>[24, 30, 45, 60, 0];

  static int _clampFrameRate(int v) => frameRates.contains(v) ? v : 30;

  void setFrameRate(int value) {
    final v = _clampFrameRate(value);
    if (v == _frameRate) return;
    _frameRate = v;
    _save();
    notifyListeners();
  }

  void setRenderScale(double value) {
    final v = value.clamp(0.5, 1.0);
    if (v == _renderScale) return;
    _renderScale = v;
    _save();
    notifyListeners();
  }

  /// Sets the master gain. The engine ramps to it internally, so this is safe
  /// to call from a scroll wheel at whatever rate the mouse produces.
  void setVolume(double value) {
    final v = value.clamp(0.05, 1.0);
    if (v == _volume) return;
    _volume = v;
    engine.gain = v;
    _save();
    notifyListeners();
  }

  void nudgeVolume(double delta) => setVolume(_volume + delta);

  void setField(double x, double y, {bool touching = true, double speed = 0}) {
    _x = x.clamp(0.0, 1.0);
    _y = y.clamp(0.0, 1.0);
    engine.setField(_x, _y);
    engine.setTouch(active: touching, speed: speed);
    _save();
    // No notifyListeners: the field changes on every pointer move and the
    // painter is already repainting every frame from the ticker. Rebuilding
    // the widget tree at 120 Hz for a value nothing in it reads would be pure
    // waste.
  }

  /// The finger is off. Notifies, unlike [setField], because the token has
  /// been changing throughout the drag and this is the moment the chrome
  /// showing it can afford to catch up.
  void endTouch() {
    engine.setTouch(active: false);
    notifyListeners();
  }

  void setMood(int value) {
    final m = value.clamp(0, neMoodCount - 1);
    if (m == _mood) return;
    _prevMood = _mood;
    _moodBlend = 0;
    _mood = m;
    engine.mood = m;
    _save();
    notifyListeners();
  }

  void cycleMood(int delta) =>
      setMood((_mood + delta + neMoodCount) % neMoodCount);

  void setPlaying(bool value) {
    if (_playing == value) return;
    _playing = value;
    engine.playing = value;
    notifyListeners();
  }

  void toggle() => setPlaying(!_playing);

  // ----------------------------------------------------------------- sleep

  Timer? _sleepFallback;

  /// The option the user picked, for highlighting it in the panel. The truth
  /// about the countdown itself is the engine's; this is only which label to
  /// draw a ring around.
  Duration? sleepChoice;

  /// Seconds until the armed sleep fires, straight from the engine. Null when
  /// disarmed - which includes "it already fired", so the UI can simply stop
  /// showing a countdown that no longer exists.
  Duration? get sleepRemaining {
    final s = engine.sleepRemaining;
    return s == null ? null : Duration(milliseconds: (s * 1000).round());
  }

  /// Arms the sleep timer, or disarms it with null.
  ///
  /// The engine owns the deadline (it must fire behind a locked screen, where
  /// this isolate may be frozen). The Dart timer here is only an echo: when
  /// the UI *is* alive at the deadline it flips [playing] so the transport
  /// and the lock-screen controls agree with the silence; when it is not,
  /// [syncFromEngine] catches up on the next frame instead.
  void setSleep(Duration? d) {
    engine.setSleep(d);
    sleepChoice = d;
    _sleepFallback?.cancel();
    _sleepFallback = d == null
        ? null
        : Timer(d + const Duration(seconds: 1), () => setPlaying(false));
    notifyListeners();
  }

  /// Called from the frame clock. The engine can stop itself (sleep landing
  /// with the app foregrounded but the fallback timer throttled, or the UI
  /// waking after a night of background audio); the transport should follow
  /// rather than claim to be playing silence.
  void syncFromEngine() {
    if (_playing && !engine.playing) {
      _playing = false;
      if (sleepChoice != null && engine.sleepRemaining == null) {
        sleepChoice = null;
        _sleepFallback?.cancel();
        _sleepFallback = null;
      }
      notifyListeners();
    }
  }

  DroneVis vis() => engine.vis();

  @override
  void dispose() {
    _sleepFallback?.cancel();
    engine.dispose();
    super.dispose();
  }
}

/// Local ease so this file does not have to pull in the widgets layer for one
/// curve.
double _ease(double t) {
  final u = t.clamp(0.0, 1.0);
  return u * u * (3 - 2 * u);
}
