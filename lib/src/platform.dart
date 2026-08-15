import 'package:flutter/services.dart';

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
