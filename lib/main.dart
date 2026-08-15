// Null Eigenvalue - a generative drone for the desktop.
//
// The whole app is one screen. Everything below is wiring: build the engine,
// restore what was playing last time, prepare the two textures the picture is
// drawn from, and get out of the way.

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:nulleig/nulleig.dart';

import 'src/drone_controller.dart';
import 'src/field_screen.dart';
import 'src/textures.dart';
import 'src/updater.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  DroneEngine engine;
  try {
    engine = DroneEngine.create();
  } catch (error) {
    runApp(_EngineFailure('$error'));
    return;
  }

  final controller = DroneController(engine);
  await controller.restore();
  controller.startAudio();

  // The builds are downloaded rather than installed from a store, so they have
  // to find out about a new version themselves. Deliberately not awaited and
  // deliberately late: the check must never be between the user and the first
  // sound.
  final updater = Updater();
  if (updater.enabled) {
    // The switch is read now and the network is touched later. Preferences are
    // already warm - the controller opened them on the way up - so by the time
    // the timer fires, check() knows whether it is allowed to run at all.
    unawaited(updater.load());
    Timer(const Duration(seconds: 6), () => unawaited(updater.check()));
  }

  final textures = await Textures.load();
  runApp(NullEigenvalueApp(
    controller: controller,
    textures: textures,
    updater: updater,
  ));
}

class NullEigenvalueApp extends StatelessWidget {
  const NullEigenvalueApp({
    super.key,
    required this.controller,
    required this.textures,
    required this.updater,
  });

  final DroneController controller;
  final Textures textures;
  final Updater updater;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Null Eigenvalue',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        brightness: Brightness.dark,
        scaffoldBackgroundColor: const Color(0xFF03070C),
        splashFactory: NoSplash.splashFactory,
        highlightColor: Colors.transparent,
      ),
      // No AnimatedBuilder here: FieldScreen subscribes to the controller
      // itself, and rebuilding it from above as well would just do the same
      // work twice per change.
      home: FieldScreen(
        controller: controller,
        textures: textures,
        updater: updater,
      ),
    );
  }
}

class _EngineFailure extends StatelessWidget {
  const _EngineFailure(this.message);

  final String message;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: Scaffold(
        backgroundColor: const Color(0xFF03070C),
        body: Center(
          child: Padding(
            padding: const EdgeInsets.all(36),
            child: Text(
              'The synthesis engine did not load.\n\n$message',
              textAlign: TextAlign.center,
              style: const TextStyle(
                fontSize: 12,
                height: 1.7,
                letterSpacing: 1.2,
                color: Color(0x99FFFFFF),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
