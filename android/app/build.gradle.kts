plugins {
    id("com.android.application")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

android {
    namespace = "com.nulleigenvalue.null_eigenvalue"
    compileSdk = flutter.compileSdkVersion
    // Pinned, and pinned to the same value as packages/nulleig/android: two
    // modules asking for two NDKs means Gradle needs both installed, and CI
    // installs exactly one.
    ndkVersion = "28.2.13676358"

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        applicationId = "com.nulleigenvalue.null_eigenvalue"
        // 24, not whatever Flutter defaults to: the engine's .so is built by
        // the plugin's CMake against minSdk 24, and a lower floor here would
        // ship an APK that installs on a device the library cannot load on.
        minSdk = 24
        targetSdk = flutter.targetSdkVersion
        // Uses the version code from pubspec.yaml. When using split APKs, 1000 * ABI_VERSION
        // is added automatically by Flutter. (https://developer.android.com/studio/build/configure-apk-splits#configure-APK-versions)
        // You can force using the value of versionCode by specifying the `-P force-version-code-ignoring-abi=true`
        // flag during build.
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    // Not a secret, and deliberately in the repository. The app is sideloaded
    // rather than sold through a store, so there is no upload key to protect;
    // what this buys is a signature that stays the same from build to build.
    // The debug key below does not: Gradle generates one per machine, so every
    // CI runner signs with a different key and a new release then refuses to
    // install over the old one - "App not installed", and the only way through
    // it is uninstalling first, which throws away the saved piece.
    //
    // Absent until the keystore workflow has been run once, hence the fallback.
    val sideloadStore = rootProject.file("nulleig-tv.jks")

    signingConfigs {
        if (sideloadStore.exists()) {
            create("sideload") {
                storeFile = sideloadStore
                storePassword = "nulleigenvalue"
                keyAlias = "nulleig"
                keyPassword = "nulleigenvalue"
            }
        }
    }

    buildTypes {
        release {
            signingConfig = if (sideloadStore.exists()) {
                signingConfigs.getByName("sideload")
            } else {
                signingConfigs.getByName("debug")
            }
            isMinifyEnabled = false
            isShrinkResources = false
        }
    }

    packaging {
        jniLibs {
            // The engine is real-time audio; letting the packager compress it
            // costs a page fault storm on first play.
            useLegacyPackaging = false
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}

flutter {
    source = "../.."
}
