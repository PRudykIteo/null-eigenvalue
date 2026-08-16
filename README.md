# Null Eigenvalue

A generative drone instrument for the phone and the desk. One screen, five
moods, no end.

It synthesises continuously — nothing is streamed and nothing is a loop — and
it keeps playing with the screen off, with play, pause and mood change on the
lock screen.

<p align="center">
  <img src="ios/Runner/Assets.xcassets/AppIcon.appiconset/Icon-App-1024x1024@1x.png" width="180" alt="">
</p>

---

## Using it

Drag anywhere. The screen is a 2D field:

- **left ↔ right** is brightness — subterranean at one edge, glassy at the other;
- **down ↕ up** is density — a bare drone at the bottom, four octaves of moving
  voices at the top.

A fast drag is heard as well as seen: it briefly excites the instrument, so the
gesture has a sound of its own and not only a result.

Tap once to show the transport and the five moods; it hides itself again after
a few seconds. While it is silent, tapping anywhere starts it.

**Kernel** is the null space, as low and as still as the thing goes.
**Manifold** is the warm, wide default. **Halo** is lydian, high, shimmering,
and the only mood that rings. **Torsion** is tense and metallic. **Limit** is
the piece as it stops: the fewest voices, the longest breaths and a
thirty-second room. **Entropy** is the classic drone — something hums,
something hisses; the noise bed is the instrument and the pitched voices are
the accompaniment, on an open fifth with no third in it at all.

From a lock screen, a headphone remote or a car, **next / previous track**
changes mood.

On a desktop it is the same screen with a pointer instead of a thumb. Moving
the mouse raises the transport and takes the cursor away again after four
seconds of stillness, so a drone left running all evening is the picture and
nothing else. The keyboard reaches everything:

| | |
|---|---|
| `space` | play / pause |
| `1`–`6` | mood |
| arrows | the field |
| wheel, or `-` / `=` | volume |
| `F` or `F11` | full screen |
| `S` | sleep timer |
| `D` | diagnostics |
| `esc` | leave full screen, or close the panel |

The same list is behind the gear, beside the sleep durations — a chromeless app
that also hides its shortcuts is just a locked door. On a Mac the media keys
and Now Playing work exactly as the lock screen does on a phone; Windows and
Linux have no equivalent to talk to.

### On a television

The set is the third shape of the same screen, and the remote is the interesting
part: it has four directions and a button, and the four directions have to mean
two different things.

| | |
|---|---|
| D-pad, picture bare | the field |
| **OK**, picture bare | raise the HUD, or start it while silent |
| D-pad, HUD up | walk the gear, the transport and the moods |
| **OK**, HUD up | use whatever has the ring round it |
| ← →, on the transport | volume |
| **Back** | close the panel, then the HUD, then the app |

So the instrument is under the D-pad whenever nothing is in the way — which is
the reason it is worth putting on a television at all — and the same four keys
become a cursor the moment there is something to point at. A ring in the accent
colour says which control the button would use; the filled dot still says which
mood is playing, and they are deliberately not the same thing, so walking across
the row costs nothing until OK is pressed.

The HUD waits eight seconds before hiding rather than four: that timeout is fair
for something you stopped pointing at and mean for something you are still
walking a D-pad across. Behind the gear the panel lays out in two columns, with
the sleep durations on the left and the level, a **diagnostics** switch and the
button legend on the right — a television has no console and no long-press, so
that switch is the only way to ask this build why it is quiet.

Remotes with a play/pause key work too; that one arrives through the media
session rather than as a key, the same way a lock screen's does.

The gear is where everything the app can be told to do now lives: sleep, level,
updates and the keys, in two columns on a window wide enough for them. The
running version sits after the wordmark at the top, at half its weight — an app
you downloaded has no store page to go and read, so "which one am I running"
has to be answerable from the app itself.

**Volume** is the desktop's own addition. A phone has a hardware rocker an inch
from the thumb already holding it; a window is one voice among a dozen other
things making noise, and the system mixer is several clicks away. The wheel is
the level — the picture has nothing to scroll, and it is where every other
player on the machine puts it — with the value appearing under the readout for
a couple of seconds and then taking itself away again. Behind the gear it is a
hairline with a dot on it, at the same weight as everything else there, for
when you want to see the number rather than nudge it. It is the engine's master
gain, underneath whatever the system says, and it is remembered between
launches.

## Installing it

Every push to `main` publishes a
[release](https://github.com/doctorspider42/null-eigenvalue/releases) with all
five builds.

**iPhone.** The `.ipa` is unsigned. The least painful route is to add this
source to AltStore once:

```
https://doctorspider42.github.io/null-eigenvalue/altstore.json
```

From then on every push to `main` shows up on the phone as an update, with no
cable and no computer in the loop — CI regenerates that manifest and publishes
it as part of the same run that builds the release.

Otherwise sign and install the `.ipa` by hand with
[AltStore](https://altstore.io) or [Sideloadly](https://sideloadly.io) using
your own Apple ID. A free account works and costs nothing; the app then has to
be re-signed every seven days, which AltStore does by itself while it is on the
same network as its desktop half.

**Android.** The `.apk` installs directly. It is signed with a debug key, so
the phone will ask you to allow installs from wherever you downloaded it.

**Android TV.** This branch builds its own APK and keeps it at one address that
does not move, because a television has no browser worth using and the Downloader
app takes a URL and nothing else:

```
https://github.com/PRudykIteo/null-eigenvalue/releases/download/tv-latest/NullEigenvalue-TV.apk
```

That is this fork, not the upstream repository — the TV release is built and published
here, and nowhere else.

Paste that into [Downloader](https://www.aftvnews.com/downloader/) on the set,
or push it from a computer on the same network:

```bash
adb connect <the TV's address>:5555
```

The address is in the set's network settings, and ADB debugging has to be turned
on first — on most Android TV and Google TV boxes that is Settings → System →
About, then clicking Build seven times, then Developer options → USB debugging.
A dialog appears on the television the first time asking whether to trust the
computer. Then:

```bash
adb install -r NullEigenvalue-TV.apk
```

The `-r` replaces an installed copy in place, which works only while the
signature stays the same. It does not by default: Gradle's debug key is
generated per machine, so every CI runner would sign with a different one and
Android would refuse the update. Running the **TV keystore** workflow once makes
a fixed key, commits it, and every build after that installs over the last. It
is deliberately not a secret — a sideloaded app has no store account behind it
and nothing to protect — and it must not be regenerated afterwards, since a new
key means the next build cannot install over this one either.

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

### Updating a desktop build

The three desktop builds ask GitHub what the newest release is — once per
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
130 Hz because a wide low end collapses on a phone speaker.

## How the picture works

Everything glowing is one white blob texture, tinted and added. The composition
is a direct reading of the synthesizer: the core is the drone, the eight orbs
are the register slices the engine publishes, a slow ring expands each time a
voice takes a new pitch, and a point of light flashes for every bell. The
engine hands the UI eight numbers and a few scalars; there is no FFT and no
second thread.

## How it is built

```
lib/                     the app: one screen, one painter, one controller
  src/platform.dart        the only file that asks which platform this is
  src/updater.dart         the desktop builds' way of noticing a new release
packages/nulleig/        the engine
  src/                     C++: synthesis, harmony, effects, and the device
  ios/Classes/*.mm         forwarders, so CocoaPods compiles src/ into the app
  macos/                   the same two forwarders and a second podspec
  windows/, linux/         CMake, one target each, same two sources
  lib/nulleig.dart         the FFI binding
windows/installer/       the Inno Setup script CI compiles
tools/render/            desktop harness: renders a WAV and measures it
tools/icons/             regenerates every launcher icon, all five platforms
```

The engine is one C++ core compiled five ways: into the iOS app binary by the
podspec, into a framework by a second podspec for the Mac, into
`libnulleig.so` by CMake for Android and again for Linux, into `nulleig.dll`
by CMake for Windows, and into a desktop program that renders WAV files. Audio is produced on the OS audio thread by
[miniaudio](https://miniaud.io) — Dart is never in the path, which is what lets
the drone survive a Flutter engine that has been suspended behind a locked
screen. Dart sets a handful of atomics and reads a few back for the visuals.

### Hearing a change without a phone

```bash
cmake -S tools/render -B tools/render/build -DCMAKE_BUILD_TYPE=Release
cmake --build tools/render/build
./tools/render/build/nulleig_render out.wav 180 --mood 2
./tools/render/build/nulleig_render tour.wav 360 --tour
```

It prints peak, per-second RMS spread, DC offset, a NaN count, a dropout count
and how often the harmony moved, and exits non-zero if any of those is wrong.
CI runs it over every mood on each push, and uploads the audio, so a change to
the DSP can be listened to before it reaches a device.

### Looking at the UI without a phone

```bash
flutter test tools/preview/preview_test.dart   # writes tools/preview/out/*.png
```

The widget tester rasterises with a real canvas, so those PNGs are what the
painter will actually draw. They are posed from hand-written engine snapshots,
which is the point: the field can be put in states that would take twenty
minutes of listening to catch by accident. (Text comes out as boxes — the test
environment has no real font. Layout and metrics are still true.)

The last three are shot at the desktop window's size rather than a phone's,
which is the one thing about that build a portrait preview cannot tell you:
whether a composition designed for a tall narrow frame still holds when the
frame is wider than it is tall, and whether the chrome scaled with it.

### Building the app

```bash
flutter pub get
flutter build apk --release
flutter build ios --release --no-codesign
flutter build windows --release
flutter build macos --release
flutter build linux --release
```

The desktop builds take `--dart-define=NE_VERSION=0.1.42`; without it the
updater stays quiet, which is what you want while working on the app. On
Windows, `flutter build` needs Developer Mode turned on — the Flutter tooling
links each plugin into the build with a symlink, and creating one is a
privileged operation otherwise:

```bash
start ms-settings:developers
```

### Building the TV APK without installing anything

The Android half of that toolchain — Flutter, the SDK, the NDK, a JDK — is
about ten gigabytes, and there is no reason for it to live on a machine that
otherwise only writes Dart. `tools/docker/Dockerfile` puts the whole thing in a
container, pinned to the same versions as the workflow, so that the build you
run locally and the build CI runs are the same build rather than two builds
that agree most of the time.

```bash
docker build -t nulleig-tv tools/docker
```

Then, from the repository root:

```bash
docker run --rm -v "${PWD}:/app" -w /app nulleig-tv flutter analyze
```

```bash
docker run --rm -v "${PWD}:/app" -w /app nulleig-tv flutter test
```

```bash
docker run --rm -v "${PWD}:/app" -v nulleig-gradle:/opt/gradle-cache -w /app nulleig-tv flutter build apk --release
```

The named volume is what makes the second build worth waiting for: `--rm` throws
the container's own filesystem away, Gradle's cache with it, and a cold cache is
the difference between about nine minutes and about five.

The APK lands in `build/app/outputs/flutter-apk/` on the host, because the
working directory is a mount rather than a copy — so it can go straight to a
television by any of the routes above without waiting for a release to be cut.

## Licence

MIT. Every dependency is permissive: Flutter (BSD-3), miniaudio (public domain
or MIT-0), `audio_service` (MIT), `shared_preferences` (BSD-3). Nothing here is
copyleft, so a build of this can be shipped under whatever terms you like.

The desktop version added no dependencies. The window switch and the updater
are each a few dozen lines against packages that would have done considerably
more than the one thing wanted.
