# Record

Android ARM64 Geode mod targeting Geometry Dash 2.2091 and Geode 6.0.0-beta.1.

## Current implementation

- OpenGL framebuffer readback from the Cocos2d director render loop.
- RGBA to YUV420 semi-planar conversion.
- Android MediaCodec H.264 encoding.
- Android MediaMuxer MP4 output.
- Pause-menu start/stop control.
- GitHub Actions Android64 build.

Output target:
`/storage/emulated/0/Android/media/com.geode.launcher/game/renders/Record.mp4`

## Known limitations in this development build

- Audio capture from FMOD is not implemented yet.
- Frame readback is synchronous and may affect frame rate.
- The output path depends on the launcher process having write access.
- Codec support and output are device-dependent and require on-device validation.
- The recorder currently captures the rendered framebuffer while active; the pause overlay may be included in frames rendered during pause.

Do not treat this development build as final until the Android64 workflow succeeds and the MP4 is tested on the target device.
