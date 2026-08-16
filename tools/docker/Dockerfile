# The Android TV build, in a box.
#
# The point is that nothing in this toolchain touches the host: Flutter, the
# Android SDK, the NDK and a JDK come to about ten gigabytes, and none of it is
# wanted on a laptop that otherwise only writes Dart. The second point is that
# the local build and the CI build are then the *same* build - every version
# below is pinned to the same literal as .github/workflows/tv-release.yml, and
# a drift between the two is what produces an APK that works on one machine and
# not on the other.
#
# Built from a plain JDK image rather than from one of the ready-made Flutter
# images, because none of them publishes the version this repository pins and
# "close enough" is exactly the drift this file exists to prevent.
#
# Build it once (from the repository root):
#     docker build -t nulleig-tv tools/docker
#
# Then:
#     docker run --rm -v "${PWD}:/app" -w /app nulleig-tv flutter analyze
#     docker run --rm -v "${PWD}:/app" -w /app nulleig-tv flutter test
#     docker run --rm -v "${PWD}:/app" -w /app nulleig-tv flutter build apk --release
#
# The APK lands in build/app/outputs/flutter-apk/ on the host, because the
# working directory is a mount rather than a copy.

# Temurin 17, matching the setup-java step in the workflow. The Android Gradle
# Plugin wants a JDK 17 and says so in a way that takes a while to read.
FROM eclipse-temurin:17-jdk

ARG FLUTTER_VERSION=3.47.0
ARG NDK_VERSION=28.2.13676358
ARG CMAKE_VERSION=3.22.1
ARG COMPILE_SDK=36
# The download URL carries this build number; there is no "latest" alias that
# resolves without one.
ARG CMDLINE_TOOLS=13114758

ENV ANDROID_HOME=/opt/android-sdk \
    ANDROID_SDK_ROOT=/opt/android-sdk \
    FLUTTER_HOME=/opt/flutter \
    PATH=/opt/flutter/bin:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools:$PATH

# git because Flutter reads its own version out of its checkout and will not
# start without it; xz-utils because the Flutter archive is a .tar.xz.
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl git unzip xz-utils zip \
 && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL -o /tmp/flutter.tar.xz \
      "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz" \
 && tar -xJf /tmp/flutter.tar.xz -C /opt \
 && rm /tmp/flutter.tar.xz

RUN curl -fsSL -o /tmp/cmdline-tools.zip \
      "https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS}_latest.zip" \
 && mkdir -p "${ANDROID_HOME}/cmdline-tools" \
 && unzip -q /tmp/cmdline-tools.zip -d "${ANDROID_HOME}/cmdline-tools" \
 && mv "${ANDROID_HOME}/cmdline-tools/cmdline-tools" "${ANDROID_HOME}/cmdline-tools/latest" \
 && rm /tmp/cmdline-tools.zip

# The NDK is named identically in packages/nulleig/android/build.gradle and in
# android/app/build.gradle.kts: two modules asking for two NDKs means Gradle
# wants both installed, and this image ships exactly one.
#
# android-35 as well as 36 because one of the plugins compiles against it, and
# a platform that is missing is not an error - Gradle simply downloads it in the
# middle of the build, which turns an offline build into a network one and hides
# a version nobody chose behind a step nobody reads.
#
# `yes |` because sdkmanager asks about licences on a terminal that is not
# there, and the licence step is allowed to fail because it exits non-zero once
# there is nothing left to accept.
RUN yes | sdkmanager --licenses > /dev/null 2>&1 || true \
 && sdkmanager --install \
      "platform-tools" \
      "platforms;android-${COMPILE_SDK}" \
      "platforms;android-35" \
      "build-tools;${COMPILE_SDK}.0.0" \
      "ndk;${NDK_VERSION}" \
      "cmake;${CMAKE_VERSION}" \
 && rm -rf /root/.android/cache

# The mounted project is owned by whichever uid the host maps in, which is not
# the uid running the build; without this, git inside the container refuses to
# look at it and Flutter's version probe fails on a working tree it can see
# perfectly well.
RUN git config --global --add safe.directory '*'

# Downloads the engine artifacts now rather than during the first build, so a
# `flutter build` on a slow connection is not silently a download.
RUN flutter config --no-analytics --android-sdk "${ANDROID_HOME}" \
 && flutter precache --android --no-ios --no-web --no-linux --no-windows --no-macos \
 && flutter doctor -v

# Gradle otherwise puts its cache inside the mounted project, where it both
# pollutes the working tree and is thrown away between runs. Keeping it in the
# image makes the second build minutes faster than the first.
ENV GRADLE_USER_HOME=/opt/gradle-cache

# A container handed no command should say what it is, rather than drop into a
# shell that looks like a hung build.
CMD ["flutter", "doctor", "-v"]
