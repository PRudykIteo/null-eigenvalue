import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:nulleig/nulleig.dart';

import 'palette.dart';

/// The only chrome in the app: a transport and five names. It is hidden by
/// default and fades back out on its own, because a control that is always on
/// screen is a control you are always looking at.
class Hud extends StatelessWidget {
  const Hud({
    super.key,
    required this.palette,
    required this.mood,
    required this.playing,
    required this.rootHz,
    required this.onMood,
    required this.onToggle,
    this.scale = 1,
    this.sleepLabel,
    this.updateLabel,
    this.onUpdateTap,
    this.volumeLabel,
    this.diagnostics,
    this.onDiagnosticsTap,
    required this.token,
    required this.liked,
    required this.onTokenTap,
    required this.onLike,
  });

  final MoodPalette palette;
  final int mood;
  final bool playing;
  final double rootHz;
  final ValueChanged<int> onMood;
  final VoidCallback onToggle;

  /// How much bigger than the base layout this is being drawn. One number for
  /// the whole HUD: the proportions are the design, and a bigger window is a
  /// bigger sheet of the same paper rather than an excuse to lay it out again.
  /// Never much past one and a half - past that the chrome starts competing
  /// with the picture it is sitting on.
  final double scale;

  /// "SLEEP 27:41" while a sleep timer runs, or null. Same register as the
  /// frequency readout: information for whoever looks, decoration for
  /// everyone else.
  final String? sleepLabel;

  /// "UPDATE 0.1.42" once a newer release has been found, and the download's
  /// progress after it has been asked for. Null otherwise, which includes
  /// every build CI did not cut.
  final String? updateLabel;
  final VoidCallback? onUpdateTap;

  /// "VOLUME 72%", for the couple of seconds after the level changes.
  ///
  /// Transient rather than permanent. The level is worth seeing while you are
  /// moving it and is clutter the rest of the time - and unlike the sleep
  /// countdown there is always a value, so a line that was always there would
  /// never be off.
  final String? volumeLabel;

  /// Shown under the readout when something is wrong with the audio device,
  /// or when the reading is asked for by long-pressing the frequency. There is
  /// no console on a downloaded build, so this is the console.
  final String? diagnostics;
  final VoidCallback? onDiagnosticsTap;

  /// The name of the piece playing. Clicking it copies it.
  final String token;

  /// Whether this exact piece is in the list. The heart is filled when it is.
  final bool liked;

  final VoidCallback onTokenTap;
  final VoidCallback onLike;

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        _Transport(
          playing: playing,
          colour: palette.accent,
          scale: scale,
          onTap: onToggle,
        ),
        SizedBox(height: 26 * scale),
        // Dots, not names. Six names at a readable size do not fit across a
        // narrow window, and the obvious fixes - a scroller, or two rows -
        // both turn the one piece of chrome in the app into a menu. A row of
        // dots with the current name under it fits any width, keeps every mood
        // one click away, and stays quiet.
        Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            for (var i = 0; i < neMoodCount; i++)
              _MoodDot(
                selected: i == mood,
                colour: palette.accent,
                scale: scale,
                onTap: () => onMood(i),
              ),
          ],
        ),
        SizedBox(height: 14 * scale),
        AnimatedSwitcher(
          duration: const Duration(milliseconds: 380),
          child: Text(
            MoodPalette.all[mood].name.toUpperCase(),
            key: ValueKey<int>(mood),
            style: TextStyle(
              fontSize: 12 * scale,
              height: 1.0,
              letterSpacing: 4.6 * scale,
              fontWeight: FontWeight.w400,
              color: palette.accent.withValues(alpha: 0.92),
            ),
          ),
        ),
        SizedBox(height: 12 * scale),
        GestureDetector(
          behavior: HitTestBehavior.opaque,
          onLongPress: onDiagnosticsTap,
          child: Padding(
            padding: EdgeInsets.symmetric(
              horizontal: 24 * scale,
              vertical: 4 * scale,
            ),
            child: Text(
              '${rootHz.toStringAsFixed(1)} Hz',
              style: TextStyle(
                fontSize: 10 * scale,
                height: 1.2,
                letterSpacing: 2.4 * scale,
                fontWeight: FontWeight.w300,
                color: Colors.white.withValues(alpha: 0.26),
              ),
            ),
          ),
        ),
        // The name of what is playing, and the one control that turns this
        // from a generator into a library. It sits directly under the
        // frequency because the two are the same kind of thing - a reading of
        // what is currently true - and above everything else here, which is
        // all either transient or a warning.
        SizedBox(height: 2 * scale),
        Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            GestureDetector(
              behavior: HitTestBehavior.opaque,
              onTap: onTokenTap,
              child: Padding(
                padding: EdgeInsets.symmetric(
                  horizontal: 10 * scale,
                  vertical: 5 * scale,
                ),
                child: Text(
                  token,
                  style: TextStyle(
                    fontFamily: 'monospace',
                    fontSize: 10 * scale,
                    height: 1.2,
                    letterSpacing: 1.6 * scale,
                    fontWeight: FontWeight.w400,
                    color: Colors.white.withValues(alpha: 0.40),
                  ),
                ),
              ),
            ),
            _HeartButton(
              filled: liked,
              colour: palette.accent,
              scale: scale,
              onTap: onLike,
            ),
          ],
        ),
        // Above the sleep countdown, because it is the line that is currently
        // moving and the one the reader just asked for.
        if (volumeLabel != null) ...<Widget>[
          SizedBox(height: 6 * scale),
          Text(
            volumeLabel!,
            style: TextStyle(
              fontSize: 10 * scale,
              height: 1.2,
              letterSpacing: 2.4 * scale,
              fontWeight: FontWeight.w300,
              color: palette.accent.withValues(alpha: 0.55),
            ),
          ),
        ],
        if (sleepLabel != null) ...<Widget>[
          SizedBox(height: 6 * scale),
          Text(
            sleepLabel!,
            style: TextStyle(
              fontSize: 10 * scale,
              height: 1.2,
              letterSpacing: 2.4 * scale,
              fontWeight: FontWeight.w300,
              color: palette.accent.withValues(alpha: 0.55),
            ),
          ),
        ],
        // The update sits under the sleep countdown and above the
        // diagnostics, which is the right order of urgency: how long the music
        // has left, then that there is a newer one of these, then why it is
        // broken. Dimmer than the mood name and no louder than the frequency -
        // it is an offer, and an offer that shouts is a nag.
        if (updateLabel != null) ...<Widget>[
          SizedBox(height: 6 * scale),
          GestureDetector(
            behavior: HitTestBehavior.opaque,
            onTap: onUpdateTap,
            child: Padding(
              padding: EdgeInsets.symmetric(
                horizontal: 24 * scale,
                vertical: 4 * scale,
              ),
              child: Text(
                updateLabel!,
                style: TextStyle(
                  fontSize: 10 * scale,
                  height: 1.2,
                  letterSpacing: 2.4 * scale,
                  fontWeight: FontWeight.w300,
                  color: palette.accent.withValues(alpha: 0.62),
                ),
              ),
            ),
          ),
        ],
        if (diagnostics != null) ...<Widget>[
          SizedBox(height: 8 * scale),
          Padding(
            padding: EdgeInsets.symmetric(horizontal: 18 * scale),
            child: Text(
              diagnostics!,
              textAlign: TextAlign.center,
              style: TextStyle(
                fontFamily: 'monospace',
                fontSize: 9 * scale,
                height: 1.35,
                color: Colors.white.withValues(alpha: 0.42),
              ),
            ),
          ),
        ],
      ],
    );
  }
}

/// The corner gear that opens the sleep panel. Drawn, like the transport,
/// because a Material icon in this picture looks like a sticker: one thin
/// outline of eight teeth and a hub, at the same stroke weight as everything
/// else. A 20-pixel glyph inside a 44-pixel target, same deal as the dots.
class GearButton extends StatelessWidget {
  const GearButton({
    super.key,
    required this.colour,
    required this.onTap,
    this.scale = 1,
  });

  final Color colour;
  final VoidCallback onTap;
  final double scale;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onTap,
      child: SizedBox(
        width: 44 * scale,
        height: 44 * scale,
        child: CustomPaint(painter: _GearPainter(colour)),
      ),
    );
  }
}

class _GearPainter extends CustomPainter {
  _GearPainter(this.colour);

  final Color colour;

  @override
  void paint(Canvas canvas, Size size) {
    final c = Offset(size.width / 2, size.height / 2);
    final paint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.1
      ..color = colour.withValues(alpha: 0.5);

    const n = 8;
    const seg = 2 * math.pi / n;
    const tw = seg * 0.44; // angular width of one tooth
    final r = size.shortestSide * 0.155; // body
    final tr = r * 1.42; // tooth tip
    final outer = Rect.fromCircle(center: c, radius: tr);
    final inner = Rect.fromCircle(center: c, radius: r);

    final path = Path()
      ..moveTo(c.dx + tr * math.cos(-tw / 2), c.dy + tr * math.sin(-tw / 2));
    for (var i = 0; i < n; i++) {
      final a = i * seg - tw / 2;
      path
        ..arcTo(outer, a, tw, false)
        ..lineTo(c.dx + r * math.cos(a + tw), c.dy + r * math.sin(a + tw))
        ..arcTo(inner, a + tw, seg - tw, false)
        ..lineTo(
            c.dx + tr * math.cos(a + seg), c.dy + tr * math.sin(a + seg));
    }
    path.close();
    canvas.drawPath(path, paint);
    canvas.drawCircle(c, r * 0.42, paint);
  }

  @override
  bool shouldRepaint(covariant _GearPainter old) => old.colour != colour;
}

class _MoodDot extends StatelessWidget {
  const _MoodDot({
    required this.selected,
    required this.colour,
    required this.onTap,
    this.scale = 1,
  });

  final bool selected;
  final Color colour;
  final VoidCallback onTap;
  final double scale;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onTap,
      // A 5-pixel dot inside a 44-pixel target: the thing you aim at is the
      // size a thumb needs, the thing you see is the size the picture needs.
      child: SizedBox(
        width: 44 * scale,
        height: 34 * scale,
        child: Center(
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 320),
            curve: Curves.easeOut,
            width: (selected ? 7 : 4) * scale,
            height: (selected ? 7 : 4) * scale,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: selected ? colour : Colors.white.withValues(alpha: 0.26),
            ),
          ),
        ),
      ),
    );
  }
}

/// The one control that turns a generator into a library.
///
/// Drawn rather than an icon, for the same reason the transport and the gear
/// are: a Material heart in this picture looks like a sticker. Outlined until
/// the piece is in the list, filled once it is, at the same stroke weight as
/// everything else here.
class _HeartButton extends StatelessWidget {
  const _HeartButton({
    required this.filled,
    required this.colour,
    required this.onTap,
    this.scale = 1,
  });

  final bool filled;
  final Color colour;
  final VoidCallback onTap;
  final double scale;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onTap,
      child: SizedBox(
        width: 34 * scale,
        height: 30 * scale,
        child: CustomPaint(
          painter: _HeartPainter(
            filled: filled,
            colour: filled ? colour : Colors.white.withValues(alpha: 0.34),
            scale: scale,
          ),
        ),
      ),
    );
  }
}

class _HeartPainter extends CustomPainter {
  _HeartPainter({
    required this.filled,
    required this.colour,
    required this.scale,
  });

  final bool filled;
  final Color colour;
  final double scale;

  @override
  void paint(Canvas canvas, Size size) {
    final w = 11.0 * scale;
    final h = 10.0 * scale;
    final cx = size.width / 2;
    final cy = size.height / 2;

    // Two arcs meeting at a point. Written out rather than taken from a font
    // so it sits on the same pixel grid as the dots beside it.
    final path = Path()
      ..moveTo(cx, cy + h * 0.42)
      ..cubicTo(cx - w * 0.62, cy - h * 0.02, cx - w * 0.50, cy - h * 0.58,
          cx - w * 0.22, cy - h * 0.42)
      ..cubicTo(cx - w * 0.08, cy - h * 0.34, cx - w * 0.02, cy - h * 0.20,
          cx, cy - h * 0.12)
      ..cubicTo(cx + w * 0.02, cy - h * 0.20, cx + w * 0.08, cy - h * 0.34,
          cx + w * 0.22, cy - h * 0.42)
      ..cubicTo(cx + w * 0.50, cy - h * 0.58, cx + w * 0.62, cy - h * 0.02,
          cx, cy + h * 0.42)
      ..close();

    canvas.drawPath(
      path,
      Paint()
        ..style = filled ? PaintingStyle.fill : PaintingStyle.stroke
        ..strokeWidth = 1.2 * scale
        ..strokeJoin = StrokeJoin.round
        ..color = colour,
    );
  }

  @override
  bool shouldRepaint(covariant _HeartPainter old) =>
      old.filled != filled || old.colour != colour || old.scale != scale;
}

class _Transport extends StatelessWidget {
  const _Transport({
    required this.playing,
    required this.colour,
    required this.onTap,
    this.scale = 1,
  });

  final bool playing;
  final Color colour;
  final VoidCallback onTap;
  final double scale;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onTap,
      child: SizedBox(
        width: 92 * scale,
        height: 92 * scale,
        child: TweenAnimationBuilder<double>(
          tween: Tween<double>(begin: 0, end: playing ? 1 : 0),
          duration: const Duration(milliseconds: 420),
          curve: Curves.easeOutCubic,
          builder: (context, t, child) => CustomPaint(
            painter: _TransportPainter(t, colour),
          ),
        ),
      ),
    );
  }
}

/// A ring and a glyph that morphs between a triangle and two bars. Drawn
/// rather than iconed so the line weight matches the rest of the picture -
/// a Material icon next to this artwork looks like it came from another app.
class _TransportPainter extends CustomPainter {
  _TransportPainter(this.t, this.colour);

  /// 0 = showing "play", 1 = showing "pause".
  final double t;
  final Color colour;

  @override
  void paint(Canvas canvas, Size size) {
    final c = Offset(size.width / 2, size.height / 2);
    final r = size.shortestSide * 0.40;

    canvas.drawCircle(
      c,
      r,
      Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.1
        ..color = colour.withValues(alpha: 0.42),
    );

    final paint = Paint()..color = colour.withValues(alpha: 0.88);
    final d = r * 0.42;

    if (t < 0.02) {
      final tri = Path()
        ..moveTo(c.dx - d * 0.5, c.dy - d)
        ..lineTo(c.dx - d * 0.5, c.dy + d)
        ..lineTo(c.dx + d, c.dy)
        ..close();
      canvas.drawPath(tri, paint);
      return;
    }

    // Two bars, growing out of the triangle's silhouette as it collapses.
    final gap = d * 0.36 * t;
    final w = d * 0.30;
    final h = d * (1 - 0.12 * (1 - t));
    for (final sign in <double>[-1, 1]) {
      final x = c.dx + sign * (gap + w * 0.5) - w * 0.5 + (1 - t) * d * 0.1;
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(x, c.dy - h, w, h * 2),
          Radius.circular(w * 0.35),
        ),
        paint..color = colour.withValues(alpha: 0.88 * math.min(1, t * 2.2)),
      );
    }
    if (t < 1) {
      final tri = Path()
        ..moveTo(c.dx - d * 0.5, c.dy - d)
        ..lineTo(c.dx - d * 0.5, c.dy + d)
        ..lineTo(c.dx + d, c.dy)
        ..close();
      canvas.drawPath(
        tri,
        Paint()..color = colour.withValues(alpha: 0.88 * (1 - t) * (1 - t)),
      );
    }
  }

  @override
  bool shouldRepaint(covariant _TransportPainter old) =>
      old.t != t || old.colour != colour;
}

/// Everything the app can be told to do, on one surface behind the gear.
///
/// It began as the sleep timer and kept its typography: a heading, then rows in
/// the mood name's face, the live one in the accent colour and the rest in the
/// HUD's whisper-grey, so even fully open this is barely a dialog. The desktop
/// added a level, an updates section and the key bindings - the last of those
/// belong here rather than on the picture, because this is the only surface in
/// the app allowed to be a list of words, and shortcuts nobody can find are
/// shortcuts nobody has.
///
/// On a window wide enough it lays out as two columns. One column was correct
/// while this was six durations; with four sections it ran off the bottom of
/// the default window, and a desktop has width going spare where it does not
/// have height.
class SettingsPanel extends StatelessWidget {
  const SettingsPanel({
    super.key,
    required this.accent,
    required this.remaining,
    required this.choice,
    required this.onPick,
    this.scale = 1,
    this.showKeys = false,
    this.volume,
    this.onVolume,
    this.updates,
    this.pieces,
    this.picture,
  });

  final Color accent;
  final Duration? remaining;
  final Duration? choice;
  final ValueChanged<Duration?> onPick;
  final double scale;

  /// The pieces section: what is playing, how to load one, and the list.
  final PiecesPanel? pieces;

  /// The two knobs that decide what the picture costs to draw.
  final PicturePanel? picture;

  /// Whether to show the key bindings, which is to say whether there is a
  /// keyboard. Also what decides the two-column layout, since it is the only
  /// section big enough to want one.
  final bool showKeys;

  /// The master level, 0..1, or null to leave the section out entirely.
  final double? volume;
  final ValueChanged<double>? onVolume;

  /// The updates section, or null to leave it out. Null on a build CI did not
  /// cut - there is nothing there to compare against a release, so an "up to
  /// date" would be a guess.
  final UpdatePanel? updates;

  static const List<int> _minutes = <int>[15, 30, 45, 60, 90];

  static const List<List<String>> _keys = <List<String>>[
    <String>['SPACE', 'PLAY / PAUSE'],
    <String>['1 - 6', 'MOOD'],
    <String>['ARROWS', 'FIELD'],
    <String>['SCROLL', 'VOLUME'],
    <String>['- / =', 'VOLUME'],
    <String>['F', 'FULL SCREEN'],
    <String>['S', 'THIS PANEL'],
    <String>['D', 'DIAGNOSTICS'],
    <String>['N', 'NEW PIECE'],
    <String>['R', 'RESTART PIECE'],
    <String>['L', 'LIKE THIS PIECE'],
    <String>['C', 'COPY ITS NAME'],
  ];

  @override
  Widget build(BuildContext context) {
    final settings = Column(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        ..._sleepSection(),
        if (volume != null) ...<Widget>[_divider(), ..._volumeSection()],
        if (picture != null) ...<Widget>[_divider(), ..._pictureSection()],
        if (updates != null) ...<Widget>[_divider(), ..._updatesSection()],
      ],
    );

    // Pieces goes in the right-hand column, above the keys. It is the section
    // with a text field and a list in it, so it wants the width, and putting
    // it under the sleep durations would push the whole left column past the
    // bottom of an ordinary window.
    final right = Column(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        if (pieces != null) ...<Widget>[..._piecesSection(), _divider()],
        ..._keysSection(),
      ],
    );

    // Scrollable regardless of the layout, because the window has a floor of
    // 480 logical pixels and the left column alone is taller than that. It
    // shrink-wraps against the loose constraints a Center hands it, so on any
    // ordinary window there is nothing to scroll and no sign that there could
    // be.
    return SingleChildScrollView(
      child: showKeys
          ? Row(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: <Widget>[
                settings,
                SizedBox(width: 56 * scale),
                right,
              ],
            )
          : settings,
    );
  }

  Widget _divider() => Padding(
        padding: EdgeInsets.symmetric(vertical: 24 * scale),
        child: Container(
          width: 200 * scale,
          height: 1,
          color: Colors.white.withValues(alpha: 0.08),
        ),
      );

  Widget _heading(String text) => Text(
        text,
        style: TextStyle(
          fontSize: 11 * scale,
          height: 1.0,
          letterSpacing: 4.6 * scale,
          fontWeight: FontWeight.w300,
          color: Colors.white.withValues(alpha: 0.45),
        ),
      );

  /// A line of information rather than a control: the level, what the last
  /// check found. Quieter than the rows above it, so the eye reads the things
  /// it can touch first.
  Widget _note(String text) => Padding(
        padding: EdgeInsets.only(top: 4 * scale),
        child: Text(
          text,
          textAlign: TextAlign.center,
          style: TextStyle(
            fontSize: 10 * scale,
            height: 1.2,
            letterSpacing: 2.4 * scale,
            fontWeight: FontWeight.w300,
            color: Colors.white.withValues(alpha: 0.28),
          ),
        ),
      );

  List<Widget> _sleepSection() {
    final armed = remaining != null;
    return <Widget>[
      Text(
        'SLEEP',
        style: TextStyle(
          fontSize: 12 * scale,
          letterSpacing: 6.5 * scale,
          fontWeight: FontWeight.w300,
          color: Colors.white.withValues(alpha: 0.45),
        ),
      ),
      SizedBox(height: 26 * scale),
      _row('OFF', selected: !armed, onTap: () => onPick(null)),
      for (final m in _minutes)
        _row(
          '$m MIN',
          selected: armed && choice?.inMinutes == m,
          onTap: () => onPick(Duration(minutes: m)),
        ),
    ];
  }

  List<Widget> _volumeSection() => <Widget>[
        _heading('VOLUME ${(volume! * 100).round()}%'),
        SizedBox(height: 16 * scale),
        _VolumeBar(
          value: volume!,
          accent: accent,
          scale: scale,
          onChanged: onVolume ?? (_) {},
        ),
      ];

  List<Widget> _updatesSection() => <Widget>[
        _heading('UPDATES'),
        SizedBox(height: 12 * scale),
        // Two rows in the same shape as the sleep durations: the switch, which
        // reads as on when it is the accent colour, and the button.
        _row(
          'AUTOMATIC',
          selected: updates!.auto,
          onTap: () => updates!.onAuto(!updates!.auto),
        ),
        _row(
          'CHECK NOW',
          // Never the accent: it is a verb, not a setting, and colouring it
          // like an armed sleep duration would read as a state.
          selected: false,
          onTap: updates!.busy ? () {} : updates!.onCheck,
        ),
        _note(updates!.status),
      ];

  /// The two knobs that decide what the picture costs to draw.
  ///
  /// These are settings rather than something chosen automatically because
  /// the right answer is a matter of taste and hardware in equal parts: a
  /// laptop on battery and a desktop with a spare graphics card want different
  /// numbers, and neither of them is a number this app can guess.
  List<Widget> _pictureSection() {
    final p = picture!;
    return <Widget>[
      _heading('PICTURE'),
      SizedBox(height: 12 * scale),
      Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: <Widget>[
          for (final r in p.rates)
            _pill(
              r == 0 ? 'FULL' : '$r',
              selected: r == p.frameRate,
              onTap: () => p.onFrameRate(r),
            ),
        ],
      ),
      _note('FRAMES PER SECOND'),
      SizedBox(height: 14 * scale),
      Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: <Widget>[
          for (final s in const <double>[0.5, 0.7, 0.85, 1.0])
            _pill(
              '${(s * 100).round()}%',
              selected: (s - p.renderScale).abs() < 0.01,
              onTap: () => p.onRenderScale(s),
            ),
        ],
      ),
      _note('DETAIL'),
    ];
  }

  /// What is playing, how to get somewhere else, and the way back.
  List<Widget> _piecesSection() {
    final p = pieces!;
    return <Widget>[
      _heading('PIECES'),
      SizedBox(height: 16 * scale),
      SelectableText(
        p.token,
        style: TextStyle(
          fontFamily: 'monospace',
          fontSize: 11 * scale,
          height: 1.2,
          letterSpacing: 1.8 * scale,
          color: accent.withValues(alpha: 0.90),
        ),
      ),
      SizedBox(height: 14 * scale),
      Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: <Widget>[
          _pill('COPY', selected: false, onTap: p.onCopy),
          _pill(p.liked ? 'LIKED' : 'LIKE',
              selected: p.liked, onTap: p.onLike),
          _pill('NEW', selected: false, onTap: p.onNew),
        ],
      ),
      SizedBox(height: 16 * scale),
      _TokenField(
        accent: accent,
        scale: scale,
        onSubmit: p.onLoad,
      ),
      if (p.entries.isNotEmpty) ...<Widget>[
        SizedBox(height: 18 * scale),
        // Bounded, and scrollable inside that bound: the panel is already the
        // tallest thing in the app and a list that grows without limit would
        // decide how tall it is.
        ConstrainedBox(
          constraints: BoxConstraints(maxHeight: 168 * scale),
          child: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: <Widget>[
                for (final entry in p.entries.reversed)
                  _LikedRow(
                    token: entry.token,
                    mood: entry.mood,
                    accent: accent,
                    playing: entry.playing,
                    scale: scale,
                    onTap: () => p.onPlay(entry.token),
                    onRemove: () => p.onRemove(entry.token),
                  ),
              ],
            ),
          ),
        ),
      ],
    ];
  }

  /// A short label in a row of them, rather than the full-width rows the sleep
  /// durations use. Three or four of these fit on a line where three of those
  /// would not.
  Widget _pill(String label,
      {required bool selected, required VoidCallback onTap}) {
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onTap,
      child: Padding(
        padding: EdgeInsets.symmetric(
          horizontal: 10 * scale,
          vertical: 8 * scale,
        ),
        child: Text(
          label,
          style: TextStyle(
            fontSize: 11 * scale,
            height: 1.0,
            letterSpacing: 2.4 * scale,
            fontWeight: FontWeight.w400,
            color: selected
                ? accent.withValues(alpha: 0.92)
                : Colors.white.withValues(alpha: 0.32),
          ),
        ),
      ),
    );
  }

  List<Widget> _keysSection() => <Widget>[
        _heading('KEYS'),
        SizedBox(height: 22 * scale),
        for (final k in _keys)
          Padding(
            padding: EdgeInsets.symmetric(vertical: 3.5 * scale),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: <Widget>[
                SizedBox(
                  width: 92 * scale,
                  child: Text(
                    k[0],
                    textAlign: TextAlign.right,
                    style: TextStyle(
                      fontSize: 10 * scale,
                      height: 1.0,
                      letterSpacing: 2.4 * scale,
                      fontWeight: FontWeight.w400,
                      color: Colors.white.withValues(alpha: 0.34),
                    ),
                  ),
                ),
                SizedBox(width: 26 * scale),
                SizedBox(
                  width: 132 * scale,
                  child: Text(
                    k[1],
                    style: TextStyle(
                      fontSize: 10 * scale,
                      height: 1.0,
                      letterSpacing: 2.4 * scale,
                      fontWeight: FontWeight.w300,
                      color: Colors.white.withValues(alpha: 0.20),
                    ),
                  ),
                ),
              ],
            ),
          ),
      ];

  Widget _row(String label, {required bool selected, required VoidCallback onTap}) {
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: onTap,
      child: Padding(
        padding: EdgeInsets.symmetric(
          horizontal: 48 * scale,
          vertical: 11 * scale,
        ),
        child: Text(
          label,
          style: TextStyle(
            fontSize: 12 * scale,
            height: 1.0,
            letterSpacing: 4.6 * scale,
            fontWeight: FontWeight.w400,
            color: selected
                ? accent.withValues(alpha: 0.92)
                : Colors.white.withValues(alpha: 0.32),
          ),
        ),
      ),
    );
  }
}

/// The level, as a hairline with a dot on it.
///
/// Not a Material slider: a chrome-heavy widget with a ripple and a thumb
/// shadow in the middle of this picture looks like it was pasted in from
/// another program. This is the same 1-pixel rule the panel's divider uses,
/// with the played part in the accent colour and a dot the size of a selected
/// mood dot - the two controls in this app that mean "here" now look alike.
class _VolumeBar extends StatelessWidget {
  const _VolumeBar({
    required this.value,
    required this.accent,
    required this.scale,
    required this.onChanged,
  });

  final double value;
  final Color accent;
  final double scale;
  final ValueChanged<double> onChanged;

  static const double _floor = 0.05;

  @override
  Widget build(BuildContext context) {
    final width = 200.0 * scale;

    void emit(double dx) =>
        onChanged((dx / width).clamp(_floor, 1.0).toDouble());

    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTapDown: (d) => emit(d.localPosition.dx),
      onHorizontalDragUpdate: (d) => emit(d.localPosition.dx),
      child: SizedBox(
        // A 44-pixel target around a 1-pixel line, the same deal as the dots.
        width: width,
        height: 44 * scale,
        child: Center(
          child: CustomPaint(
            size: Size(width, 12 * scale),
            painter: _VolumeBarPainter(value, accent),
          ),
        ),
      ),
    );
  }
}

class _VolumeBarPainter extends CustomPainter {
  _VolumeBarPainter(this.value, this.accent);

  final double value;
  final Color accent;

  @override
  void paint(Canvas canvas, Size size) {
    final y = size.height / 2;
    final x = size.width * value.clamp(0.0, 1.0);

    canvas.drawLine(
      Offset(0, y),
      Offset(size.width, y),
      Paint()
        ..strokeWidth = 1
        ..color = Colors.white.withValues(alpha: 0.14),
    );
    canvas.drawLine(
      Offset(0, y),
      Offset(x, y),
      Paint()
        ..strokeWidth = 1
        ..color = accent.withValues(alpha: 0.55),
    );
    canvas.drawCircle(
      Offset(x, y),
      3.5,
      Paint()..color = accent.withValues(alpha: 0.92),
    );
  }

  @override
  bool shouldRepaint(covariant _VolumeBarPainter old) =>
      old.value != value || old.accent != accent;
}

/// What the panel needs to know about updates, and what to call when the user
/// touches one of the two rows.
///
/// A plain value rather than a reference to the Updater, so hud.dart keeps
/// knowing nothing about HTTP and the preview harness can pose the section in
/// states - mid-check, rate-limited - that are awkward to reach for real.
class UpdatePanel {
  const UpdatePanel({
    required this.auto,
    required this.busy,
    required this.status,
    required this.onAuto,
    required this.onCheck,
  });

  /// Whether the app looks for a new version by itself.
  final bool auto;

  /// A check or a download is in flight; the button should not start another.
  final bool busy;

  /// One line under the two rows: what the last check found, or what this
  /// build is. Always present - a section that is sometimes three rows and
  /// sometimes two makes the panel jump when it is opened.
  final String status;

  final ValueChanged<bool> onAuto;
  final VoidCallback onCheck;
}

/// Where a token someone sent you goes.
///
/// Stateful for one reason: it has to be able to say "that is not a token".
/// Silently doing nothing is the worst possible answer here - the user cannot
/// tell a rejected paste from a piece that happens to sound similar, and the
/// checksum exists precisely so that the app knows the difference.
class _TokenField extends StatefulWidget {
  const _TokenField({
    required this.accent,
    required this.onSubmit,
    this.scale = 1,
  });

  final Color accent;
  final bool Function(String) onSubmit;
  final double scale;

  @override
  State<_TokenField> createState() => _TokenFieldState();
}

class _TokenFieldState extends State<_TokenField> {
  final TextEditingController _text = TextEditingController();
  bool _rejected = false;

  @override
  void dispose() {
    _text.dispose();
    super.dispose();
  }

  void _submit(String value) {
    if (value.trim().isEmpty) return;
    if (widget.onSubmit(value)) {
      _text.clear();
      setState(() => _rejected = false);
    } else {
      setState(() => _rejected = true);
    }
  }

  @override
  Widget build(BuildContext context) {
    final scale = widget.scale;
    return SizedBox(
      width: 250 * scale,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          TextField(
            controller: _text,
            onSubmitted: _submit,
            onChanged: (_) {
              if (_rejected) setState(() => _rejected = false);
            },
            textAlign: TextAlign.center,
            cursorColor: widget.accent,
            style: TextStyle(
              fontFamily: 'monospace',
              fontSize: 11 * scale,
              letterSpacing: 1.8 * scale,
              color: Colors.white.withValues(alpha: 0.80),
            ),
            decoration: InputDecoration(
              isDense: true,
              hintText: 'NE1-....-....-....',
              hintStyle: TextStyle(
                fontFamily: 'monospace',
                fontSize: 11 * scale,
                letterSpacing: 1.8 * scale,
                color: Colors.white.withValues(alpha: 0.18),
              ),
              contentPadding: EdgeInsets.symmetric(vertical: 8 * scale),
              // The same hairline the divider and the volume bar use, so the
              // one place in this app that takes typing still looks like it
              // belongs to the picture rather than to a form.
              enabledBorder: UnderlineInputBorder(
                borderSide: BorderSide(
                  color: _rejected
                      ? const Color(0xFFCC6666)
                      : Colors.white.withValues(alpha: 0.12),
                ),
              ),
              focusedBorder: UnderlineInputBorder(
                borderSide: BorderSide(
                  color: _rejected
                      ? const Color(0xFFCC6666)
                      : widget.accent.withValues(alpha: 0.60),
                ),
              ),
            ),
          ),
          SizedBox(height: 6 * scale),
          Text(
            _rejected ? 'NOT A PIECE NAME' : 'PASTE A PIECE',
            style: TextStyle(
              fontSize: 9 * scale,
              height: 1.2,
              letterSpacing: 2.4 * scale,
              fontWeight: FontWeight.w300,
              color: _rejected
                  ? const Color(0xFFCC6666).withValues(alpha: 0.75)
                  : Colors.white.withValues(alpha: 0.24),
            ),
          ),
        ],
      ),
    );
  }
}

/// One liked piece: click the name to play it, the cross to forget it.
class _LikedRow extends StatelessWidget {
  const _LikedRow({
    required this.token,
    required this.mood,
    required this.accent,
    required this.playing,
    required this.onTap,
    required this.onRemove,
    this.scale = 1,
  });

  final String token;
  final String mood;
  final Color accent;
  final bool playing;
  final VoidCallback onTap;
  final VoidCallback onRemove;
  final double scale;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: onTap,
          child: Padding(
            padding: EdgeInsets.symmetric(vertical: 6 * scale),
            child: SizedBox(
              width: 210 * scale,
              child: Row(
                children: <Widget>[
                  Text(
                    token,
                    style: TextStyle(
                      fontFamily: 'monospace',
                      fontSize: 10 * scale,
                      height: 1.2,
                      letterSpacing: 1.2 * scale,
                      color: playing
                          ? accent.withValues(alpha: 0.90)
                          : Colors.white.withValues(alpha: 0.42),
                    ),
                  ),
                  const Spacer(),
                  Text(
                    mood.toUpperCase(),
                    style: TextStyle(
                      fontSize: 9 * scale,
                      height: 1.2,
                      letterSpacing: 1.6 * scale,
                      fontWeight: FontWeight.w300,
                      color: Colors.white.withValues(alpha: 0.22),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
        GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: onRemove,
          child: Padding(
            padding: EdgeInsets.symmetric(
              horizontal: 10 * scale,
              vertical: 6 * scale,
            ),
            child: Text(
              '×',
              style: TextStyle(
                fontSize: 13 * scale,
                height: 1.0,
                color: Colors.white.withValues(alpha: 0.26),
              ),
            ),
          ),
        ),
      ],
    );
  }
}

/// One row of the liked list. Plain values, for the same reason [UpdatePanel]
/// is: hud.dart does not know what a Piece is and has no reason to learn.
class LikedEntry {
  const LikedEntry({
    required this.token,
    required this.mood,
    required this.playing,
  });

  final String token;

  /// The mood's name, which is most of what tells two of these apart at a
  /// glance - the token itself is twelve characters of base32 and reads as
  /// noise until you already know which one you are looking for.
  final String mood;

  /// Whether this is the piece currently playing.
  final bool playing;
}

/// The pieces section, as plain values and callbacks.
class PiecesPanel {
  const PiecesPanel({
    required this.token,
    required this.liked,
    required this.entries,
    required this.onCopy,
    required this.onLike,
    required this.onNew,
    required this.onLoad,
    required this.onPlay,
    required this.onRemove,
  });

  /// The name of what is playing.
  final String token;

  /// Whether that piece is in [entries].
  final bool liked;

  final List<LikedEntry> entries;

  final VoidCallback onCopy;
  final VoidCallback onLike;
  final VoidCallback onNew;

  /// Takes whatever was typed or pasted. Returns false if it was not a token,
  /// which is what the field turns into a message rather than silence.
  final bool Function(String) onLoad;

  final ValueChanged<String> onPlay;
  final ValueChanged<String> onRemove;
}

/// What the picture costs, as two numbers the user owns.
class PicturePanel {
  const PicturePanel({
    required this.frameRate,
    required this.renderScale,
    required this.rates,
    required this.onFrameRate,
    required this.onRenderScale,
  });

  /// Frames per second, or 0 for whatever the display asks.
  final int frameRate;

  /// The fraction of the window's pixels the field is drawn at, 0.5..1.
  final double renderScale;

  final List<int> rates;
  final ValueChanged<int> onFrameRate;
  final ValueChanged<double> onRenderScale;
}
