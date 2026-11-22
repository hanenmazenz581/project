# StudioCut Pro — native editor foundation

StudioCut Pro is an original Windows video editor project. It is not Filmora and does not contain Filmora assets. This repository is being rebuilt as a native Qt/C++ application rather than extending the old Python demo.

## Current build scope
- Native Qt 6 desktop shell with a dark editing workspace.
- Media-bin import for video, audio, and image files.
- Preview playback through Qt Multimedia.
- Separate sequential Video Track 1, Video Track 2 (overlay), and Audio Track 1 lanes.
- Video, overlay, and audio clip reordering/removal, split-at-playhead, and per-clip in/out trimming.
- FFmpeg export composites Video Track 2 over Video Track 1; transparent PNG overlays retain alpha.
- Project save/open as JSON, including audio-track clips.
- FFmpeg export with selectable HD/Full HD/QHD/4K, 24/25/30/50/60 fps, H.264/H.265, and quality presets.
- Sequential video assembly and sequential external-audio assembly, muxed into an MP4 with AAC audio.
- Build workflow for Windows x64.

## Not implemented yet
Independent clip start positions, overlap editing, additional stacked tracks, transitions, keyframes, GPU effects, mixing source-video audio with external tracks, title animation, AI models, proxy media, and full Filmora-level parity are not claimed as complete. V1 and V2 are sequential lanes; V2 is composited over V1 from the beginning of the sequence.

## Build locally
Install Qt 6 (Widgets, Multimedia, MultimediaWidgets) and FFmpeg, then:
```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```
Set `FFMPEG_EXECUTABLE` in CMake or put `ffmpeg.exe` on PATH.

## Export limitation
The current renderer concatenates clips on Video Track 1, concatenates Video Track 2 as an overlay layer, and independently concatenates clips on Audio Track 1, then muxes the result. It does not yet support clip overlap, precise independent placement, source-video audio mixing, transitions, or layered video compositing.


## CI
Windows x64 package is built automatically by GitHub Actions on pushes to this branch. The current native preview build is an early developer build, not a feature-complete Filmora replacement.
