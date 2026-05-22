# Render Review Sets

Repo-local detached render-review artifacts for `ray_tracing` live here.

This lane is for local docs and review writeups inside the repository. It is
not the live visualizer website pipeline. Live website publication goes through
the `visualizer-run/v1` staging/import flow owned by
`skills/codework-visualizer-drop/`.

Use this lane for:

- one authored scene state
- one detached render request
- one selected output frame for repo-doc inspection
- one copied render summary for downstream review

Typical contents per set:

- `request.json`
- `preview.bmp`
- optional `preview.png`
- `summary.json`
- `index.md`

These sets are intended to mirror one completed detached run in a stable
repo-doc form without keeping the full private run root exposed.

Current published sets:

- `grime_screen_motion_review_v2/`
