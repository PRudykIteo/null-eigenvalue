# Null Eigenvalue

A generative drone instrument for the desktop. One screen, six moods, no end.

It synthesises continuously — nothing is streamed and nothing is a loop — and
every piece it makes has a name you can write down, come back to, and send to
somebody else.

<p align="center">
  <img src="macos/Runner/Assets.xcassets/AppIcon.appiconset/app_icon_512.png" width="180" alt="">
</p>

---

## Using it

Drag anywhere. The screen is a 2D field:

- **left ↔ right** is brightness — subterranean at one edge, glassy at the other;
- **down ↕ up** is density — a bare drone at the bottom, four octaves of moving
  voices at the top.

A fast drag is heard as well as seen: it briefly excites the instrument, so the
gesture has a sound of its own and not only a result.

Click once to show the transport and the six moods; it hides itself again after
a few seconds. While it is silent, clicking anywhere starts it.

**Kernel** is the null space, as low and as still as the thing goes.
**Manifold** is the warm, wide default. **Halo** is lydian, high, shimmering,
and the only mood that rings. **Torsion** is tense and metallic. **Limit** is
the piece as it stops: the fewest voices, the longest breaths and a
thirty-second room. **Entropy** is the classic drone — something hums,
something hisses; the noise bed is the instrument and the pitched voices are
the accompaniment, on an open fifth with no third in it at all.

Moving the mouse raises the transport and takes the cursor away again after four
seconds of stillness, so a drone left running all evening is the picture and
nothing else. The keyboard reaches everything:

| | |
|---|---|
| `space` | play / pause |
| `1`–`6` | mood |
| arrows | the field |
| wheel, or `-` / `=` | volume |
| `F` or `F11` | full screen |
| `S` | sleep timer, and everything else behind the gear |
| `D` | diagnostics |
| `N` | a new piece |
| `R` | this piece again from the top |
| `L` | keep this piece |
| `C` | copy its name |
| `esc` | leave full screen, or close the panel |

The same list is behind the gear, beside the sleep durations — a chromeless app
that also hides its shortcuts is just a locked door.

The gear is where everything the app can be told to do now lives: sleep, level,
pieces, picture, updates and the keys, in two columns on a window wide enough
for them. The
running version sits after the wordmark at the top, at half its weight — an app
you downloaded has no store page to go and read, so "which one am I running"
has to be answerable from the app itself.

**Volume.** A window is one voice among a dozen other things making noise, and
the system mixer is several clicks away. The wheel is
the level — the picture has nothing to scroll, and it is where every other
player on the machine puts it — with the value appearing under the readout for
a couple of seconds and then taking itself away again. Behind the gear it is a
hairline with a dot on it, at the same weight as everything else there, for
when you want to see the number rather than nudge it. It is the engine's master
gain, underneath whatever the system says, and it is remembered between
launches.

## Pieces

Every piece has a name, shown under the frequency and looking like this:

```
NE1-K7M2-9QRX-4B2F
```

Give that to somebody with the same version of the app and they hear what you
heard, from the beginning. `C` copies it, and the field behind the gear takes
one back — paste the whole message it arrived in if you like, it will find the
name inside. `N` starts a piece nobody has heard. `R` plays this one again from
the top. `L` keeps it, and kept pieces are listed behind the gear, one click to
play.

Twelve characters is enough because a piece is not much information: a 32-bit
seed, which mood, and where the field was. **All three are in there**, and that
is the point. The seed alone decides which notes the voices walk to and where
the bells fall — but the mood decides the scale they walk in, the register and
half the effects, and the field decides brightness and density, and through
them the filter, the timbre, how many voices sound at all and how often a bell
arrives. Two people on the same seed with their pointers in different corners
are not listening to the same thing.

So the name is a bookmark of all four numbers. Loading one puts the field where
it was; after that the field is an instrument again, and moving it makes a
variant — which is what the readout then says, live. There is one honest gap:
after a mood change the name describes what you would hear if you *started*
from these settings, which is not quite what is playing, because a mood is
walked into over a minute rather than cut to. That is deliberate, and it is the
only thing a name could mean for something with no end.

The `NE1` in front is a version, not decoration. Everything after it is an
index into the engine's behaviour, so if the harmony weights or a mood's
parameters ever change, that becomes `NE2` and old names are refused by name
rather than quietly playing something else.

## Installing it

Every push to `main` publishes a
[release](https://github.com/doctorspider42/null-eigenvalue/releases) with all
three builds.

**Windows.** `NullEigenvalue-Setup.exe` installs into your own profile and
needs no administrator. It is not signed, so SmartScreen will say it does not
recognise the publisher — More info, then Run anyway.

**macOS.** `NullEigenvalue.dmg`. The app is signed to itself and not notarised,
because there are no Apple credentials in this repo, so the first launch has to
be right-click → Open rather than a double-click. Alternatively:

```bash
xattr -dr com.apple.quarantine "/Applications/Null Eigenvalue.app"
```

**Linux.** `NullEigenvalue-x86_64.AppImage`. `chmod +x` and run it; it needs
GTK 3, and finds ALSA, PulseAudio, PipeWire or JACK by itself at run time.

### Updating

All three builds ask GitHub what the newest release is — once per
launch, at most once every six hours, several seconds after the audio is
already running so a slow network can never be between you and the first
sound. When there is a newer one, a line appears under the frequency readout;
clicking it fetches that platform's installer and hands it over. Windows
installs silently and reopens the app, macOS mounts the disk image, and Linux
replaces the AppImage in place and asks to be restarted.

Behind the gear, **UPDATES** has the two controls that go with that.
**AUTOMATIC** turns the unprompted check off; **CHECK NOW** asks anyway,
ignoring both the switch and the six hours, because a check you asked for out
loud is not the thing either of them was protecting you from. Underneath is
what the last one found — up to date, a version on offer, or that GitHub could
not be reached.

Only that line ever mentions a check that found nothing. The picture is told
about a newer version existing and about a download going wrong, and about
nothing else: an app that interrupts itself to say nothing happened is an app
you stop reading.

A build made on your own machine has no version baked into it, shows `DEV` by
the wordmark, and never offers anything. Only a build CI cut compares itself to
a release.

## How the music works

The hard part of a generative drone is not making a nice sound. It is making
one that is still interesting in twenty minutes without ever doing anything
sudden. Two obvious designs both fail: a fixed chord is a texture you have
heard all of within ninety seconds, and a chord *progression* announces itself
as a loop the second time round.

So there is no progression.

**A fixed root, and voices that breathe.** Fourteen voices sit above a drone
root. Two of them *are* the drone and never leave. The other twelve each fade
in and out on a period of their own, between 24 and 86 seconds, and those
periods are spaced by the golden ratio — no two alike, no two in any simple
ratio. The *combination* of which voices are sounding therefore has a
recurrence time measured in days. Nothing repeats, and no random number was
involved in the timing.

**Voices only change pitch while silent.** Every time a voice comes back in it
takes a new note. Because it is inaudible at that moment, nothing has to
crossfade or glide: the harmony can move as much as it likes and you never hear
a change happen. You notice, half a minute later, that the chord is somewhere
else.

**The new note is chosen by how it sounds.** Candidates come from the mood's
scale, weighted by consonance against the voices that are *currently audible*
(weighted by how audible each one is), by how well the note fits that voice's
preferred register, and against repeating what that voice played last time.
That is what keeps twelve independently wandering voices reading as one harmony
instead of a cluster.

**The root itself walks.** Every few minutes it moves by a fifth or a third,
with a restoring pull so it cannot wander out of its register. Voices keep
their offsets, so the whole field transposes at once — the one event in the
piece big enough to notice while it is happening.

**Weather.** Brightness, density and the size of the room are also pushed
around by pink noise, generated at a quarter of a hertz and smoothed over tens
of seconds. 1/f has structure at every timescale, which is why the piece gets
stretches of calm and then a swell, where an LFO would just have a visible
period.

Underneath all of that: mip-mapped band-limited wavetables (a drone gives you
all day to hear aliasing), a per-voice detuned unison with slow random drift,
an 8-line feedback delay network with a Hadamard mixing matrix for the tail — a
comb bank rings metallic long before the twenty seconds this needs — with
per-line damping, modulated line lengths and a pitch-shifted feedback path for
shimmer, a ping-pong delay, an ensemble chorus, and a bass sum to mono below
130 Hz to keep the low end from smearing on a small speaker.

Every modulation source in there is a rotating unit vector rather than a call
to `sin` — there were fifteen of those per sample between the reverb's
breathing line lengths, the chorus taps and the shimmer windows, all of them
computing oscillators that run at a fraction of a hertz.

## How the picture works

Everything glowing is one white blob texture, tinted and added. The composition
is a direct reading of the synthesizer: the core is the drone, the eight orbs
are the register slices the engine publishes, a slow ring expands each time a
voice takes a new pitch, and a point of light flashes for every bell. The
engine hands the UI eight numbers and a few scalars; there is no FFT and no
second thread.

What it costs is almost entirely blended pixels — a dozen very large soft
shapes, several of them bigger than the window — so the two settings behind the
gear are the two that matter. **Frames per second** is capped at 30 by default:
nothing here moves faster than a spring settling over a third of a second, and
on a 144 Hz display the cap alone is most of the app's cost. **Detail** draws
the field into a smaller image and stretches it back, which scales the cost by
the square; there is no edge anywhere in this picture to lose. The dither over
the top is always drawn at full size, and doubles as dither for the upscale.

Turning the diagnostics on with `D` reports both sides: `dsp2.1%` is the share
of each audio buffer the synthesis spends, and `ui30fps` is what the picture is
actually managing. Those two numbers are there so that "the app is CPU heavy"
is a question with an answer rather than a guess.

## How it is built

```
lib/                     the app: one screen, one painter, one controller
  src/piece.dart           a piece, and the token that names it
  src/platform.dart        the window, as one switch
  src/updater.dart         how a downloaded build notices a new release
packages/nulleig/        the engine
  src/                     C++: synthesis, harmony, effects, and the device
  macos/                   two forwarders and a podspec
  windows/, linux/         CMake, one target each, same two sources
  lib/nulleig.dart         the FFI binding
windows/installer/       the Inno Setup script CI compiles
tools/render/            offline harness: renders a WAV and measures it
tools/icons/             regenerates every launcher icon
```

The engine is one C++ core compiled four ways: into a framework by the podspec
for the Mac, into `libnulleig.so` by CMake for Linux, into `nulleig.dll` by
CMake for Windows, and into a desktop program that renders WAV files. Audio is
produced on the OS audio thread by [miniaudio](https://miniaud.io) — Dart is
never in the path, so a janking or garbage-collecting UI cannot interrupt the
sound. Dart sets a handful of atomics and reads a few back for the visuals.

The iOS and Android builds were dropped in favour of doing one platform
properly; the C++ still compiles for both, and the CI jobs are commented out
rather than deleted, so the way back is uncommenting them and restoring the
platform folders from git.

### Hearing a change without opening the app

```bash
cmake -S tools/render -B tools/render/build -DCMAKE_BUILD_TYPE=Release
cmake --build tools/render/build
./tools/render/build/nulleig_render out.wav 180 --mood 2
./tools/render/build/nulleig_render tour.wav 360 --tour
```

It prints peak, per-second RMS spread, DC offset, a NaN count, a dropout count,
how often the harmony moved, and how long the render took as a share of one
core — which is the honest way to find out what the synthesis costs, with no
window in the measurement. It exits non-zero if any of those is wrong.
CI runs it over every mood on each push, and uploads the audio, so a change to
the DSP can be listened to before it reaches a device.

### Looking at the UI without launching it

```bash
flutter test tools/preview/preview_test.dart   # writes tools/preview/out/*.png
```

The widget tester rasterises with a real canvas, so those PNGs are what the
painter will actually draw. They are posed from hand-written engine snapshots,
which is the point: the field can be put in states that would take twenty
minutes of listening to catch by accident. (Text comes out as boxes — the test
environment has no real font. Layout and metrics are still true.)

The last three are shot at the size the runners open at rather than a narrow
one, which is the thing a portrait preview cannot tell you: whether a
composition designed for a tall narrow frame still holds when the frame is
wider than it is tall, and whether the chrome scaled with it.

### Building the app

```bash
flutter pub get
flutter build windows --release
flutter build macos --release
flutter build linux --release
```

They take `--dart-define=NE_VERSION=0.1.42`; without it the
updater stays quiet, which is what you want while working on the app. On
Windows, `flutter build` needs Developer Mode turned on — the Flutter tooling
links each plugin into the build with a symlink, and creating one is a
privileged operation otherwise:

```bash
start ms-settings:developers
```

## Licence

MIT. Every dependency is permissive: Flutter (BSD-3), miniaudio (public domain
or MIT-0), `shared_preferences` (BSD-3). Nothing here is
copyleft, so a build of this can be shipped under whatever terms you like.

The desktop version added no dependencies. The window switch and the updater
are each a few dozen lines against packages that would have done considerably
more than the one thing wanted.
