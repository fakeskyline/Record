# Record

An Android ARM64 Geode mod for Geometry Dash. The project targets Geometry Dash 2.2091 and Geode 6.0.0-beta.1.

## Target platform

- Geometry Dash: 2.2091
- Geode: 6.0.0-beta.1
- Android ABI: arm64-v8a
- Android minimum SDK: 23
- Build system: CMake + Geode SDK
- CI: GitHub Actions

## Planned recorder behavior

- Record the Geometry Dash render output only, not the entire device screen.
- Encode H.264 video and AAC game audio into MP4.
- Capture music and sound effects from the game's audio engine.
- Add a compact icon control to the in-game pause menu.
- Save recordings to:
  `/storage/emulated/0/Android/media/com.geode.launcher/game/renders/`
- Detect unsupported encoder resolutions and frame rates instead of silently producing broken files.

## Build

Every push to `main` starts the Android64 GitHub Actions build. Run it manually from the Actions tab with **Build Record (Android64) → Run workflow**.

The resulting `.geode` package is uploaded as the `Record-Android64` artifact.

## Current implementation status

This commit establishes the Geode project, target metadata, and reproducible Android64 CI build. It is **not yet a functional recorder**: the gameplay frame capture, audio tap, H.264/AAC encoding, MP4 muxing, and pause-menu controls still need implementation and on-device validation. No recording controls are exposed until the pipeline can create a valid file.
