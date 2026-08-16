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

    // The published build is signed with one fixed key, because Gradle's debug
    // key is generated per machine: every CI runner would sign with a different
    // one and a new release would then refuse to install over the old app -
    // "App not installed", and the only way through it is uninstalling first,
    // which throws away the saved piece.
    //
    // The key lives in the repository's secrets rather than in the repository.
    // The workflow decodes it to this path and puts the password in the
    // environment; both are absent everywhere else, and a build that cannot
    // find them falls back to the debug key rather than failing. That is the
    // right default for a local build - it produces a working APK to look at,
    // and one that deliberately cannot pose as the published app.
    val sideloadStore = rootProject.file("nulleig-tv.jks")
    val sideloadPassword: String? = System.getenv("TV_KEYSTORE_PASSWORD")
    val signSideload = sideloadStore.exists() && !sideloadPassword.isNullOrBlank()

    signingConfigs {
        if (signSideload) {
            create("sideload") {
                storeFile = sideloadStore
                storePassword = sideloadPassword
                keyAlias = "nulleig"
                keyPassword = sideloadPassword
            }
        }
    }

    buildTypes {
        release {
            signingConfig = if (signSideload) {
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
