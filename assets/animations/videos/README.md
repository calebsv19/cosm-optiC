# assets/animations/videos

Rendered videos exported from runtime frame sequences.

- default output path: `data/runtime/videos/output.mp4`
- generation path: `make video` (or `src/tools/make_video.c::MakeVideo`)

Outputs are produced by `src/tools/make_video.c::MakeVideo`, which shells out to FFmpeg using the active frame directory and animation settings.
