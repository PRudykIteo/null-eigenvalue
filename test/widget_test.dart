// These tests deliberately do not build the app.
//
// Everything interesting about Null Eigenvalue is either behind FFI - and
// `flutter test` does not link the engine - or it is a picture, and a golden
// test of a nebula that is animated by a random walk is a test that fails on
// Tuesdays. What is worth pinning down here is the boundary: the constants
// Dart and C++ have to agree on, the colour maths the whole look rests on, and
// the token format that decides whether a shared piece is the same piece.

import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:null_eigenvalue/src/palette.dart';
import 'package:null_eigenvalue/src/piece.dart';
import 'package:null_eigenvalue/src/updater.dart';
import 'package:nulleig/nulleig.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('every mood the engine knows has a palette', () {
    // If these ever disagree, the app indexes past the end of the palette list
    // the first time someone taps the last mood.
    expect(MoodPalette.all.length, neMoodCount);
  });

  test('palette names match the engine order', () {
    const expected = <String>[
      'Kernel',
      'Manifold',
      'Halo',
      'Torsion',
      'Limit',
      'Entropy',
    ];
    expect(MoodPalette.all.map((p) => p.name).toList(), expected);
  });

  test('band colours run from deep to accent', () {
    final p = MoodPalette.all[1];
    final low = p.forBand(0, neBands, 0);
    final high = p.forBand(neBands - 1, neBands, 0);
    expect(low, isNot(equals(high)));
    // Luminance has to increase with register or the picture reads upside
    // down: the low voices would be the bright ones.
    expect(_luma(high), greaterThan(_luma(low)));
  });

  test('brightness lifts the colour without leaving the palette', () {
    final p = MoodPalette.all[2];
    final dull = p.forBand(3, neBands, 0);
    final bright = p.forBand(3, neBands, 1);
    expect(_luma(bright), greaterThan(_luma(dull)));
  });

  test('palette lerp is stable at the ends', () {
    final a = MoodPalette.all[0];
    final b = MoodPalette.all[3];
    expect(MoodPalette.lerp(a, b, 0).bg, a.bg);
    expect(MoodPalette.lerp(a, b, 1).bg, b.bg);
    expect(MoodPalette.lerp(a, b, -5).bg, a.bg);
    expect(MoodPalette.lerp(a, b, 5).bg, b.bg);
  });

  test('an empty vis snapshot is the right shape', () {
    expect(DroneVis.empty.bands.length, neBands);
    expect(DroneVis.empty.gate, 0);
  });

  // A token is the only thing in this app that travels between two people, so
  // it is the only thing where being subtly wrong is worse than failing. What
  // matters is that it survives a round trip exactly, that it survives being
  // pasted out of a chat window, and that anything damaged on the way is
  // refused rather than played as some other piece.
  group('the piece token', () {
    const piece = Piece(seed: 0x4E756C6C, mood: 3, x: 0.75, y: 0.25);

    test('is the shape people have to be able to read down a phone', () {
      expect(piece.token, matches(RegExp(r'^NE1(-[0-9A-HJKMNP-TV-Z]{4}){3}$')));
    });

    test('survives a round trip', () {
      final back = Piece.parse(piece.token);
      expect(back, isNotNull);
      expect(back!.seed, piece.seed);
      expect(back.mood, piece.mood);
      expect(back.x, closeTo(piece.x, 1 / 63));
      expect(back.y, closeTo(piece.y, 1 / 63));
    });

    test('is exact once it has been through a token', () {
      // The field is quantised, so a piece read back from a token has to be
      // the fixed point of the whole trip - otherwise a shared piece drifts a
      // fraction every time it is passed on.
      final once = Piece.parse(piece.token)!;
      final twice = Piece.parse(once.token)!;
      expect(twice, once);
      expect(twice.token, once.token);
    });

    test('keeps the extremes of the field reachable', () {
      for (final corner in const <List<double>>[
        <double>[0, 0],
        <double>[1, 1],
        <double>[0, 1],
      ]) {
        final p = Piece(seed: 1, mood: 0, x: corner[0], y: corner[1]);
        final back = Piece.parse(p.token)!;
        expect(back.x, corner[0]);
        expect(back.y, corner[1]);
      }
    });

    test('survives the whole range of seeds', () {
      for (final seed in <int>[0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]) {
        final back = Piece.parse(Piece(seed: seed, mood: 1, x: 0.5, y: 0.5).token);
        expect(back?.seed, seed, reason: 'seed $seed');
      }
    });

    test('arrives out of a chat window in one piece', () {
      final t = piece.token;
      for (final messy in <String>[
        t.toLowerCase(),
        '  $t  ',
        t.replaceAll('-', ''),
        t.replaceAll('-', ' '),
        'listen to this: $t',
      ]) {
        expect(Piece.parse(messy), piece.quantised, reason: messy);
      }
    });

    test('refuses anything that is not a token', () {
      expect(Piece.parse(''), isNull);
      expect(Piece.parse('NE1'), isNull);
      expect(Piece.parse('hello'), isNull);
      // Right shape, wrong version: a token from an engine whose harmony has
      // moved on would play something else entirely, so it is refused by name.
      expect(Piece.parse(piece.token.replaceFirst('NE1', 'NE2')), isNull);
      // Truncated.
      expect(Piece.parse(piece.token.substring(0, piece.token.length - 1)), isNull);
    });

    test('refuses a token with a character mistyped', () {
      // The one failure the checksum is for. Walk every position and confirm
      // that a single wrong character is almost never accepted - and never
      // accepted as a *different* piece without being noticed.
      final t = piece.token.replaceAll('-', '').substring(3);
      var accepted = 0;
      for (var i = 0; i < t.length; i++) {
        for (final c in '0123456789ABCDEFGHJKMNPQRSTVWXYZ'.split('')) {
          if (c == t[i]) continue;
          final bad = 'NE1${t.substring(0, i)}$c${t.substring(i + 1)}';
          if (Piece.parse(bad) != null) accepted++;
        }
      }
      // Eight bits of checksum, so about one in 256 single-character slips
      // gets through. Over 12 positions x 31 substitutions that is a couple.
      expect(accepted, lessThan(8), reason: '$accepted of 372 slips accepted');
    });

    test('a different seed is a different token', () {
      final a = const Piece(seed: 1, mood: 0, x: 0.5, y: 0.5).token;
      final b = const Piece(seed: 2, mood: 0, x: 0.5, y: 0.5).token;
      expect(a, isNot(b));
    });

    test('the mood is part of what is shared', () {
      // It decides the scale, the register and whether there are bells at all.
      // Two tokens differing only in mood must not collide.
      final tokens = <String>{
        for (var m = 0; m < neMoodCount; m++)
          Piece(seed: 99, mood: m, x: 0.5, y: 0.5).token,
      };
      expect(tokens.length, neMoodCount);
    });

    test('so is the field', () {
      final a = const Piece(seed: 7, mood: 1, x: 0.1, y: 0.9).token;
      final b = const Piece(seed: 7, mood: 1, x: 0.9, y: 0.1).token;
      expect(a, isNot(b));
    });
  });

  // The desktop updater's one piece of pure logic, and the one place it could
  // do harm: say yes wrongly and it downloads an installer nobody asked for.
  group('version comparison', () {
    test('a higher patch is newer', () {
      expect(isNewerVersion('0.1.42', '0.1.41'), isTrue);
      expect(isNewerVersion('0.1.41', '0.1.42'), isFalse);
    });

    test('the same version is not newer', () {
      expect(isNewerVersion('0.1.7', '0.1.7'), isFalse);
    });

    test('components are compared as numbers, not as text', () {
      // The bug this exists to prevent: CI's patch number is the run number,
      // so it goes past 9 on the tenth push and string ordering would then
      // stop offering updates for good.
      expect(isNewerVersion('0.1.10', '0.1.9'), isTrue);
      expect(isNewerVersion('0.2.0', '0.10.0'), isFalse);
    });

    test('a missing component counts as zero', () {
      expect(isNewerVersion('0.2', '0.1.9'), isTrue);
      expect(isNewerVersion('0.1', '0.1.0'), isFalse);
    });

    test('anything unparseable is not newer', () {
      // A malformed tag must be able to fail in one direction only.
      expect(isNewerVersion('nightly', '0.1.7'), isFalse);
      expect(isNewerVersion('0.1.7-rc1', '0.1.6'), isFalse);
      expect(isNewerVersion('0.1.8', ''), isFalse);
    });
  });

  // The switch, and only the switch. Everything past the guard is an HTTPS
  // request, and a unit test that reaches GitHub is a unit test that fails
  // whenever the runner has no network - so what is pinned here is the one
  // thing that must hold offline: that "off" means the app does not go to the
  // network on its own, and that it is still off next launch.
  group('the automatic update check', () {
    test('does nothing while it is switched off', () async {
      SharedPreferences.setMockInitialValues(<String, Object>{
        'updateAuto': false,
      });
      final updater = Updater(currentVersion: '0.1.0');
      await updater.load();
      expect(updater.auto, isFalse);

      // Returns before touching the network, so this is safe with no route to
      // the internet - and if the guard ever regresses, the stage moves off
      // idle and this fails.
      await updater.check();
      expect(updater.stage, UpdateStage.idle);
    });

    test('is on unless it has been turned off, and stays off', () async {
      SharedPreferences.setMockInitialValues(<String, Object>{});
      final first = Updater(currentVersion: '0.1.0');
      await first.load();
      expect(first.auto, isTrue, reason: 'the default is to look');

      await first.setAuto(false);

      final next = Updater(currentVersion: '0.1.0');
      await next.load();
      expect(next.auto, isFalse, reason: 'the switch outlives the launch');
    });
  });
}

double _luma(Color c) => 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
