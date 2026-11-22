# StudioCut Pro engineering roadmap

## Milestone 0.5 — native editing foundation
- [x] Native Qt desktop shell
- [x] Media import and preview
- [x] Sequential clip timeline
- [x] Clip trim, split, reorder, remove
- [x] Project JSON save/open
- [x] Basic FFmpeg H.264 video export
- [ ] Verify Windows CI build and run on a real Windows desktop

## Milestone 0.6 — real timeline engine
- Multi-track timeline with independent video/audio/title tracks
- Frame-accurate timebase and snapping
- Undo/redo command stack
- Persistent media metadata and thumbnails/waveforms
- Audio stream preservation and track mixing

## Milestone 0.7 — compositing and effects
- Transitions with preview/render parity
- Per-clip transform, opacity, crop, speed
- Keyframes and interpolation
- Color correction and LUT pipeline
- Chroma key and masks

## Milestone 0.8 — titles and audio
- Editable title layers and motion presets
- Subtitle editor and SRT import/export
- Audio mixer, fades, gain, EQ, noise reduction

## Milestone 0.9 — AI tools
- Local model manager with model licenses and hardware checks
- Background segmentation, speech transcription, translation, object tracking
- Clear offline/online status and downloadable model packs

## Milestone 1.0 — release quality
- Crash recovery and autosave
- Proxy media and cache management
- Export presets, hardware encoding, queue
- Installer, signing, compatibility testing and regression suite

A feature is considered complete only after it works in the preview, survives project save/reopen, and renders correctly in an exported file.
