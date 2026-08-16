import 'dart:io';

import 'package:flutter/services.dart';

/// The three desktops, as one question.
///
/// The app is one screen on every platform and this file is deliberately the
/// only place that asks which one it is running on. What actually differs is
/// small: a phone has orientation and system bars, a desktop has a window, a
/// pointer that hovers and a keyboard.
bool get isDesktop => Platform.isWindows || Platform.isMacOS || Platform.isLinux;

bool get isMobile => Platform.isAndroid || Platform.isIOS;

/// Whether the instrument is running on a television.
///
/// This branch exists only to put it on one, so here Android *is* the TV:
/// there is no phone build on this tree to tell it apart from. A source that
/// shipped both would have to ask `UiModeManager` across a channel and wait for
/// the answer before the first frame, which is a startup cost paid to learn
/// something this tree knows at compile time.
///
/// What actually differs is the whole input story - no pointer, no keyboard, a
/// D-pad four inches from someone's hand and three metres from the screen - so
/// this flag is read rather more than the other two.
bool get isTv => Platform.isAndroid;

/// Whether `audio_service` has an implementation here.
///
/// It ships Android, iOS and macOS. On the Mac that is worth having: it puts
/// the mood in Now Playing and makes the F7/F8/F9 media keys change mood and
/// pause, exactly as the lock screen does on a phone. Windows and Linux have
/// no such plugin, and asking anyway costs eight seconds of startup waiting
/// for a channel nobody is answering.
bool get hasMediaSession =>
    Platform.isAndroid || Platform.isIOS || Platform.isMacOS;

/// The window, as far as this app cares about it: one switch.
///
/// Implemented in each runner rather than taken from a package. It is about
/// twenty lines of native code per platform against a dependency that would
/// pull three plugins in to do considerably more than the one thing wanted -
/// and a drone you leave running deserves a way to get the title bar off the
/// picture.
class AppWindow {
  const AppWindow._();

  static const MethodChannel _channel =
      MethodChannel('nulleigenvalue/window');

  static bool _fullscreen = false;

  /// Whether the window is currently filling the screen.
  static bool get isFullscreen => _fullscreen;

  /// Flips it, and returns the state afterwards.
  static Future<bool> toggleFullscreen() => setFullscreen(!_fullscreen);

  static Future<bool> setFullscreen(bool value) async {
    if (!isDesktop) return false;
    try {
      final result = await _channel.invokeMethod<bool>(
        'setFullscreen',
        <String, Object?>{'value': value},
      );
      _fullscreen = result ?? _fullscreen;
    } on PlatformException {
      // A runner that does not answer is a window that does not go fullscreen.
      // Nothing else in the app depends on this.
    } on MissingPluginException {
      _fullscreen = false;
    }
    return _fullscreen;
  }
}
