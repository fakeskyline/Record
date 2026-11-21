# Record

A Geometry Dash Geode mod for Android that aims to record gameplay to MP4.

## Target

- Geometry Dash 2.2091
- Geode 6.0.0-beta.1
- Android ARM64 (`arm64-v8a`)

## Recording requirements

- Capture Geometry Dash gameplay only (not the device screen).
- MP4 output with H.264 video and AAC audio.
- Capture game audio, including music and sound effects.
- Save recordings under `Android/media/com.geode.launcher/game/renders/`.
- Provide Start/Stop controls in the in-game pause menu.

**Status:** project initialization. Recording is not implemented yet; builds must not be treated as a working recorder until video capture, audio capture, and MP4 muxing are implemented and tested on-device.
