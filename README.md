# StudioCut Pro — native editor foundation

StudioCut Pro is an original Windows video editor project. It is not Filmora and does not contain Filmora assets. This repository is being rebuilt as a native Qt/C++ application rather than extending the old Python demo.

## Current build scope
- Native Qt 6 desktop shell with a dark editing workspace.
- Media-bin import for video, audio, and image files.
- Preview playback through Qt Multimedia.
- Timeline clip assembly, clip reordering/removal, split-at-playhead, and per-clip in/out trimming.
- Project save/open as JSON.
- FFmpeg-backed sequential video export (video stream, H.264 MP4).
- Build workflow for Windows x64.

## Not implemented yet
Multi-track compositing, transitions, keyframes, GPU effects, audio mixing, title animation, AI models, proxy media, and full Filmora-level parity are not claimed as complete. They are planned engineering milestones.

## Build locally
Install Qt 6 (Widgets, Multimedia, MultimediaWidgets) and FFmpeg, then:
```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```
Set `FFMPEG_EXECUTABLE` in CMake or put `ffmpeg.exe` on PATH.

## Export limitation
The first export engine concatenates trimmed clips in sequence and encodes H.264 video. Audio mixing and multi-track compositing are scheduled for later milestones.


## CI
Windows x64 package is built automatically by GitHub Actions on pushes to this branch. The current native preview build is an early developer build, not a feature-complete Filmora replacement.
