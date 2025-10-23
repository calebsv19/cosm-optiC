# Animations

Captured frames and rendered videos live here. The animation loop in `src/app/animation.c` writes BMP frames into the directory configured by `animSettings.frameDir`, and `src/tools/make_video.c` stitches those frames into MP4 files.

## Subdirectories
- `default/` – Current working frame cache. Files follow the `frame_####.bmp` pattern and are produced when deep render mode is enabled. The count and naming align with FFmpeg's expectations (`frame_0000.bmp`, `frame_0001.bmp`, …).
- `deep_render/` – Reserved output target for high-quality batch renders; presently empty.
- `Vids/` – Saved MP4 renders. `output.mp4`, `output copy.mp4`, and `1stOfMany.mp4` are sample exports created by FFmpeg after frame capture.

See each subdirectory README for additional context and per-file notes.
