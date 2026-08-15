#!/usr/bin/env python3
"""Generates the launcher icons for the three desktops.

The icon is the app: a dark field with one soft orb in it and a thin ring
around the place the sound comes from. Drawn rather than exported from a design
tool so that changing the palette is a one-line edit and every size is
regenerated consistently - an icon set assembled by hand always ends up with
one stale 29x29 in it.

    python tools/icons/make_icons.py

Requires Pillow and numpy.
"""

import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

BG_TOP = (0x03, 0x06, 0x0B)
BG_BOTTOM = (0x08, 0x0E, 0x18)
DEEP = (0x0E, 0x2A, 0x4A)
MID = (0x1B, 0x7C, 0x97)
ACCENT = (0x5C, 0xE0, 0xCC)

# Supersample, then downsample with a good filter. At 20x20 the ring is a
# fraction of a pixel wide and drawing it directly leaves a dotted circle.
SS = 4


def radial(size, cx, cy, radius, falloff=2.2):
    """A soft disc, alpha 1 at the centre falling to 0 at `radius`."""
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    d = np.sqrt(((x + 0.5) / size - cx) ** 2 + ((y + 0.5) / size - cy) ** 2)
    t = np.clip(1.0 - d / radius, 0.0, 1.0)
    return t ** falloff


def ring(size, cx, cy, radius, width):
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    d = np.sqrt(((x + 0.5) / size - cx) ** 2 + ((y + 0.5) / size - cy) ** 2)
    return np.clip(1.0 - np.abs(d - radius) / width, 0.0, 1.0) ** 1.4


def render(size):
    n = size * SS
    img = np.zeros((n, n, 3), dtype=np.float32)

    ramp = np.linspace(0.0, 1.0, n, dtype=np.float32)[:, None]
    for c in range(3):
        img[:, :, c] = (BG_TOP[c] + (BG_BOTTOM[c] - BG_TOP[c]) * ramp) / 255.0

    def add(mask, colour, gain):
        m = mask[:, :, None] * gain
        img[:] += m * (np.array(colour, dtype=np.float32) / 255.0)

    # Three concentric glows sharing one centre, offset only slightly so the
    # form reads as lit rather than as two overlapping circles. An earlier
    # version put the highlight a long way off centre and the seam between the
    # two blobs was the first thing you saw.
    add(radial(n, 0.50, 0.55, 0.52, 2.4), DEEP, 1.20)
    add(radial(n, 0.49, 0.52, 0.38, 2.1), MID, 0.80)
    add(radial(n, 0.455, 0.470, 0.22, 2.2), ACCENT, 0.62)
    add(ring(n, 0.5, 0.52, 0.340, 0.010), ACCENT, 0.42)
    add(ring(n, 0.5, 0.52, 0.445, 0.006), ACCENT, 0.12)

    img = np.clip(img, 0.0, 1.0)
    out = Image.fromarray((img * 255).astype(np.uint8), mode="RGB")
    return out.resize((size, size), Image.LANCZOS)


# The names in macos/Runner/Assets.xcassets/AppIcon.appiconset/Contents.json.
# Xcode reads that manifest, not the directory, so these have to match it
# exactly or the build quietly ships a missing icon.
MACOS_SIZES = [16, 32, 64, 128, 256, 512, 1024]

# What goes into the .ico. Windows picks the nearest of these for the title
# bar, the task bar, Alt-Tab and the desktop; 256 is the one Explorer uses for
# a large-icon view, and leaving it out is what makes an app look blurry in
# exactly one place.
WINDOWS_ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def main():
    cache = {}

    def get(size):
        if size not in cache:
            cache[size] = render(size)
        return cache[size]

    macos_dir = os.path.join(
        ROOT, "macos", "Runner", "Assets.xcassets", "AppIcon.appiconset"
    )
    if os.path.isdir(macos_dir):
        for size in MACOS_SIZES:
            get(size).save(os.path.join(macos_dir, "app_icon_%d.png" % size))
            print("macos", size)

    windows_icon = os.path.join(
        ROOT, "windows", "runner", "resources", "app_icon.ico"
    )
    if os.path.isdir(os.path.dirname(windows_icon)):
        # Pillow builds the whole multi-resolution .ico from one image, but it
        # downsamples internally with its own filter. Handing it the largest
        # render and letting it do that produced a visibly softer 16x16 than
        # rendering at 16 does, so every size is rendered at its own scale and
        # appended - the same reason this script exists at all.
        frames = [get(s) for s in WINDOWS_ICO_SIZES]
        frames[-1].save(
            windows_icon,
            format="ICO",
            sizes=[(s, s) for s in WINDOWS_ICO_SIZES],
            append_images=frames[:-1],
        )
        print("windows", "app_icon.ico", WINDOWS_ICO_SIZES)

    # Linux has no icon slot in the Flutter runner - the desktop environment
    # takes one from the .desktop file instead, and the AppImage the release
    # workflow builds is where that gets assembled.
    linux_icon = os.path.join(ROOT, "linux", "nulleigenvalue.png")
    if os.path.isdir(os.path.dirname(linux_icon)):
        get(512).save(linux_icon)
        print("linux", "nulleigenvalue.png", 512)

    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
