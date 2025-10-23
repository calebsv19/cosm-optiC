# Animations › Vids

Rendered videos exported from captured frame sequences.

- `1stOfMany.mp4` – Early sample render generated from a previous frame batch.
- `output.mp4` – Latest automatically generated video when deep render completed with `animSettings.autoMP4 = true`.
- `output copy.mp4` – Manual duplicate of `output.mp4` kept for comparison or archival.

Outputs are produced by `src/tools/make_video.c::MakeVideo`, which shells out to FFmpeg using the active frame directory and animation settings.
