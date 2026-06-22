#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
WORK_ROOT="$(ray_tracing_test_reset_work_root repo_doc_redaction "$ROOT_DIR")"
RUN_ROOT="$WORK_ROOT/render_run"
FRAME_DIR="$RUN_ROOT/ray_tracing/output/frames"
RENDER_SET="r4s3_render_redaction_fixture"
MATERIAL_SET="r4s3_material_redaction_fixture"
RENDER_DOC_DIR="$ROOT_DIR/docs/render_review_sets/$RENDER_SET"
MATERIAL_DOC_DIR="$ROOT_DIR/docs/material_preview_sets/$MATERIAL_SET"
FAKE_CLI="$WORK_ROOT/fake_bin/ray_tracing_material_preview_headless"

rm -rf "$RENDER_DOC_DIR" "$MATERIAL_DOC_DIR"
mkdir -p "$FRAME_DIR" "$RUN_ROOT/ray_tracing" "$(dirname "$FAKE_CLI")"

python3 - "$FRAME_DIR/frame_0000.png" "$WORK_ROOT/material_preview.bmp" <<'PY'
import base64
import sys

png = (
    b"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8"
    b"/x8AAwMCAO+/p9sAAAAASUVORK5CYII="
)
data = base64.b64decode(png)
for path in sys.argv[1:]:
    with open(path, "wb") as handle:
        handle.write(data)
PY

cat > "$RUN_ROOT/ray_request.json" <<JSON
{
  "schema_version": "ray_tracing_agent_render_request_v1",
  "scene": {
    "runtime_scene_path": "/Users/calebsv/Desktop/CodeWork/private_scene_runtime.json"
  },
  "output": {
    "root": "$RUN_ROOT/ray_tracing/output",
    "video": {
      "path": "/private/tmp/private_render.mp4"
    }
  },
  "progress": {
    "summary_path": "$RUN_ROOT/ray_tracing/render_summary.json"
  }
}
JSON

cat > "$RUN_ROOT/ray_tracing/render_summary.json" <<JSON
{
  "schema_version": "ray_tracing_headless_summary_v1",
  "request_path": "$RUN_ROOT/ray_request.json",
  "private_drop": "$ROOT_DIR/../_private_workspace_artifacts/agent_runs/private_drop",
  "nested": {
    "log": "/private/tmp/ray_tracing/private.log"
  }
}
JSON

cat > "$WORK_ROOT/material_request.json" <<JSON
{
  "schema": "ray_tracing_material_preview_request_v1",
  "runtime_scene_path": "/Users/calebsv/Desktop/CodeWork/private_material_scene.json",
  "object_id": "fixture_object",
  "output_path": "$WORK_ROOT/material_preview.bmp",
  "summary_path": "$WORK_ROOT/material_summary.json"
}
JSON

cat > "$WORK_ROOT/material_summary.json" <<JSON
{
  "runtime_scene_path": "/Users/calebsv/Desktop/CodeWork/private_material_scene.json",
  "workspace_artifact": "$ROOT_DIR/../_private_workspace_artifacts/material/private_summary.json",
  "variants": [
    {
      "label": "private /private/tmp/label"
    }
  ]
}
JSON

cat > "$FAKE_CLI" <<'SH'
#!/usr/bin/env sh
exit 0
SH
chmod +x "$FAKE_CLI"

"$ROOT_DIR/tools/publish_render_review_set.sh" \
  --run-root "$RUN_ROOT" \
  --set-id "$RENDER_SET" \
  --title "R4 S3 Render Redaction" \
  --frame frame_0000.png

RAY_TRACING_MATERIAL_PREVIEW_CLI="$FAKE_CLI" \
  "$ROOT_DIR/tools/publish_material_preview_set.sh" \
  --request "$WORK_ROOT/material_request.json" \
  --set-id "$MATERIAL_SET" \
  --title "R4 S3 Material Redaction"

for json_file in \
  "$RENDER_DOC_DIR/request.json" \
  "$RENDER_DOC_DIR/summary.json" \
  "$MATERIAL_DOC_DIR/request.json" \
  "$MATERIAL_DOC_DIR/summary.json"; do
  test -f "$json_file"
  if grep -E '/Users/|/private/|_private_workspace_artifacts/' "$json_file"; then
    echo "private path leaked into $json_file" >&2
    exit 1
  fi
  grep -q '<redacted-local-path:' "$json_file"
done

grep -q 'redacted `request.json`' "$ROOT_DIR/docs/render_review_sets/README.md"

rm -rf "$RENDER_DOC_DIR" "$MATERIAL_DOC_DIR"
python3 - "$ROOT_DIR/docs/render_review_sets/README.md" "$ROOT_DIR/docs/render_review_sets" <<'PY'
import os
import sys

readme_path, root_dir = sys.argv[1:3]
entries = []
for name in sorted(os.listdir(root_dir)):
    if name == "README.md":
        continue
    candidate = os.path.join(root_dir, name, "index.md")
    if os.path.isfile(candidate):
        entries.append(name)

with open(readme_path, "w", encoding="utf-8") as f:
    f.write("# Render Review Sets\n\n")
    f.write("Repo-local detached render-review artifacts for `ray_tracing` live here.\n\n")
    f.write("This lane is for local docs and review writeups inside the repository. It is\n")
    f.write("not the live visualizer website pipeline. Live website publication goes through\n")
    f.write("the `visualizer-run/v1` staging/import flow owned by\n")
    f.write("`skills/codework-visualizer-drop/`.\n\n")
    f.write("Use this lane for:\n\n")
    f.write("- one authored scene state\n")
    f.write("- one redacted detached render request\n")
    f.write("- one selected output frame for repo-doc inspection\n")
    f.write("- one redacted render summary for downstream review\n\n")
    f.write("Typical contents per set:\n\n")
    f.write("- redacted `request.json`\n")
    f.write("- `preview.bmp`\n")
    f.write("- optional `preview.png`\n")
    f.write("- redacted `summary.json`\n")
    f.write("- `index.md`\n\n")
    f.write("These sets are intended to mirror one completed detached run in a stable\n")
    f.write("repo-doc form without keeping the full private run root exposed. Public JSON\n")
    f.write("copies redact local/private paths such as `/Users/...`, `/private/...`, and\n")
    f.write("`_private_workspace_artifacts/...`.\n\n")
    f.write("Current published sets:\n\n")
    for entry in entries:
        f.write(f"- `{entry}/`\n")
PY

echo "ray tracing repo-doc redaction passed"
