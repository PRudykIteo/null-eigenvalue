import 'dart:math' as math;

import 'package:nulleig/nulleig.dart';

/// One piece of music, as the four numbers that produce it.
///
/// The seed on its own is not enough, and that is the whole reason this class
/// exists rather than an `int`. The engine's random streams decide which notes
/// the voices walk to and where the bells fall, but the *mood* decides the
/// scale they walk in, the register, and half the effects; and the field
/// decides brightness and density, and through them the filter, the timbre,
/// how many voices are sounding at all and how often a bell arrives. Two
/// people on the same seed with their pointers in different corners are not
/// listening to the same thing, and a token that only carried the seed would
/// make that everybody's problem.
///
/// So a token carries all four. Loading one puts the field where it was; after
/// that the field is an instrument again and moving it makes a variant, which
/// is what [token] then says.
class Piece {
  const Piece({
    required this.seed,
    required this.mood,
    required this.x,
    required this.y,
  });

  /// The engine seed, an unsigned 32-bit value.
  final int seed;

  /// Which instrument this is. 0..[neMoodCount] - 1.
  final int mood;

  /// The field. `x` is brightness, `y` is density, both 0..1.
  final double x;
  final double y;

  /// A piece nobody has heard yet.
  factory Piece.random({
    required int mood,
    required double x,
    required double y,
    math.Random? rng,
  }) {
    final r = rng ?? math.Random();
    return Piece(
      seed: r.nextInt(1 << 32),
      mood: mood,
      x: x,
      y: y,
    ).quantised;
  }

  /// This piece as a token would reproduce it.
  ///
  /// The field survives a token at six bits per axis, so a piece that has been
  /// through one is not quite the piece that was playing. Rounding on the way
  /// *in* rather than only on the way out means what is on screen is what the
  /// token says, instead of the two disagreeing in the last decimal place.
  Piece get quantised => Piece(
        seed: seed & 0xFFFFFFFF,
        mood: mood,
        x: _decodeAxis(_encodeAxis(x)),
        y: _decodeAxis(_encodeAxis(y)),
      );

  Piece withField(double newX, double newY) =>
      Piece(seed: seed, mood: mood, x: newX, y: newY);

  Piece withMood(int newMood) =>
      Piece(seed: seed, mood: newMood, x: x, y: y);

  /// `NE1-K7M2-9QRX-4B2F`, which is short enough to read down a phone.
  ///
  /// Sixty bits: the seed, the mood, six bits per field axis, five spare, and
  /// an eight-bit checksum - in Crockford's base32, so I/L/O cannot be
  /// mistaken for 1/1/0 and there is no U for a token to spell something with.
  ///
  /// The `NE1` in front is not decoration. Everything after it is an index
  /// into the engine's behaviour, so changing the harmony weights, a mood's
  /// parameters or the order the voices are set up in changes what a token
  /// produces. When that happens the prefix goes to NE2 and old tokens are
  /// refused by name, rather than quietly playing something else.
  String get token {
    final payload = _payload;
    final full = (payload << _checksumBits) | _checksum(payload);

    final chars = List<String>.filled(_tokenChars, '0');
    for (var i = _tokenChars - 1; i >= 0; i--) {
      chars[i] = _alphabet[(full >> ((_tokenChars - 1 - i) * 5)) & 0x1F];
    }
    final body = chars.join();
    return '$_prefix-${body.substring(0, 4)}'
        '-${body.substring(4, 8)}'
        '-${body.substring(8, 12)}';
  }

  /// Reads a token back, or null if it is not one.
  ///
  /// Liberal about how it arrives - any case, any separators, any surrounding
  /// whitespace - because the realistic way one of these travels is pasted out
  /// of a chat window with something else stuck to it. Strict about the two
  /// things that decide whether the music is right: the version prefix and the
  /// checksum.
  static Piece? parse(String input) {
    final cleaned = input.toUpperCase().replaceAll(RegExp(r'[^0-9A-Z]'), '');
    // Every place the prefix appears, not only the start: a token arrives with
    // "listen to this:" in front of it far more often than it arrives alone,
    // and the checksum is what decides whether a candidate is really one.
    for (var at = cleaned.indexOf(_prefix);
        at >= 0;
        at = cleaned.indexOf(_prefix, at + 1)) {
      final piece = _decode(cleaned, at + _prefix.length);
      if (piece != null) return piece;
    }
    return null;
  }

  static Piece? _decode(String s, int start) {
    if (start + _tokenChars > s.length) return null;

    var full = 0;
    for (var i = start; i < start + _tokenChars; i++) {
      final v = _value(s.codeUnitAt(i));
      if (v < 0) return null;
      full = (full << 5) | v;
    }

    final payload = full >> _checksumBits;
    if ((full & ((1 << _checksumBits) - 1)) != _checksum(payload)) return null;

    final mood = (payload >> 17) & 0x7;
    // A token from a build that knows more moods than this one names an
    // instrument that is not here. Clamping would play the wrong piece
    // silently, which is exactly what the checksum above exists to avoid.
    if (mood >= neMoodCount) return null;

    return Piece(
      seed: (payload >> 20) & 0xFFFFFFFF,
      mood: mood,
      x: _decodeAxis((payload >> 11) & 0x3F),
      y: _decodeAxis((payload >> 5) & 0x3F),
    );
  }

  /// Whether [input] is a token this build can play, for enabling a button.
  static bool looksValid(String input) => parse(input) != null;

  int get _payload =>
      ((seed & 0xFFFFFFFF) << 20) |
      ((mood & 0x7) << 17) |
      (_encodeAxis(x) << 11) |
      (_encodeAxis(y) << 5);

  @override
  bool operator ==(Object other) =>
      other is Piece &&
      other.seed == seed &&
      other.mood == mood &&
      other._payload == _payload;

  @override
  int get hashCode => _payload.hashCode;

  @override
  String toString() => token;

  // ------------------------------------------------------------- the format

  static const String _prefix = 'NE1';
  static const int _tokenChars = 12;
  static const int _checksumBits = 8;

  /// Crockford's base32: no I, L, O or U.
  static const String _alphabet = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';

  static int _value(int codeUnit) {
    // The substitutions Crockford specifies, which are the whole point of
    // choosing it: someone reading a token aloud says "oh" and "ell".
    if (codeUnit == 0x4F) return 0; // O
    if (codeUnit == 0x49 || codeUnit == 0x4C) return 1; // I, L
    return _alphabet.indexOf(String.fromCharCode(codeUnit));
  }

  /// 63 rather than 64 as the divisor, so that both ends of the field are
  /// exactly representable and a token made at the corner reproduces there.
  static int _encodeAxis(double v) =>
      (v.clamp(0.0, 1.0) * 63).round().clamp(0, 63);

  static double _decodeAxis(int v) => v / 63.0;

  static int _checksum(int payload) {
    // FNV-1a over the payload's seven bytes. It is not a cryptographic
    // anything; it is here to turn a mistyped character into "that is not a
    // token" instead of into a different piece of music.
    var h = 0x811c9dc5;
    for (var shift = 48; shift >= 0; shift -= 8) {
      h ^= (payload >> shift) & 0xFF;
      h = (h * 0x01000193) & 0xFFFFFFFF;
    }
    return h & 0xFF;
  }
}
