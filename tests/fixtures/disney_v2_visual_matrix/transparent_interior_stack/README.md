# Transparent Interior Stack Visual Matrix

Canonical D2.18 proof scene for transparent/interior preservation. The scene
uses two transparent vertical panes in front of colored interior geometry so the
Disney v2 denoise pass can be checked for stable-area smoothing without erasing
camera-visible transparent layers or behind-surface detail.

Use `request_disney_v2_denoise_off_12.json` and
`request_disney_v2_denoise_on_12.json` for the apples-to-apples comparison.
Both requests use the same scene, camera, resolution, and `temporal_frames=12`;
only `render.denoise_enabled` changes.
