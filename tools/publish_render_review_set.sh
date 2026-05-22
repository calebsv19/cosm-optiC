#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

RUN_ROOT=""
SET_ID=""
TITLE=""
FRAME_NAME="frame_0000.bmp"

while [ $# -gt 0 ]; do
  case "$1" in
    --run-root)
      RUN_ROOT="$2"
      shift 2
      ;;
    --set-id)
      SET_ID="$2"
      shift 2
      ;;
    --title)
      TITLE="$2"
      shift 2
      ;;
    --frame)
      FRAME_NAME="$2"
      shift 2
      ;;
    *)
      echo "usage: $0 --run-root /abs/run/root --set-id review_set_id [--title 'Title'] [--frame frame_0000.bmp]" >&2
      exit 2
      ;;
  esac
done

if [ -z "$RUN_ROOT" ] || [ -z "$SET_ID" ]; then
  echo "usage: $0 --run-root /abs/run/root --set-id review_set_id [--title 'Title'] [--frame frame_0000.bmp]" >&2
  exit 2
fi

if [ -z "$TITLE" ]; then
  TITLE="$SET_ID"
fi

python3 - "$RUN_ROOT" "$SET_ID" "$TITLE" "$FRAME_NAME" "$ROOT_DIR" <<'PY'
import json
import os
import shutil
import subprocess
import sys

run_root, set_id, title, frame_name, root_dir = sys.argv[1:6]
request_path = os.path.join(run_root, "ray_request.json")
summary_path = os.path.join(run_root, "ray_tracing", "render_summary.json")
frame_path = os.path.join(run_root, "ray_tracing", "output", "frames", frame_name)
dest_dir = os.path.join(root_dir, "docs", "render_review_sets", set_id)
readme_path = os.path.join(root_dir, "docs", "render_review_sets", "README.md")
os.makedirs(dest_dir, exist_ok=True)

dest_bmp = os.path.join(dest_dir, "preview.bmp")
dest_png = os.path.join(dest_dir, "preview.png")

shutil.copy2(request_path, os.path.join(dest_dir, "request.json"))
shutil.copy2(frame_path, dest_bmp)
if os.path.exists(summary_path):
    shutil.copy2(summary_path, os.path.join(dest_dir, "summary.json"))

png_written = False
if shutil.which("ffmpeg"):
    try:
        subprocess.check_call(
            ["ffmpeg", "-y", "-i", dest_bmp, dest_png],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        png_written = True
    except subprocess.CalledProcessError:
        png_written = False

with open(os.path.join(dest_dir, "index.md"), "w", encoding="utf-8") as f:
    f.write(f"# {title}\n\n")
    f.write("Generated detached render review set for `ray_tracing`.\n\n")
    f.write("- source request: `request.json`\n")
    f.write(f"- selected frame: `{frame_name}`\n")
    f.write("- rendered preview: `preview.bmp`\n")
    if png_written:
        f.write("- web preview: `preview.png`\n")
    if os.path.exists(summary_path):
        f.write("- render summary: `summary.json`\n")

entries = []
if os.path.isdir(os.path.dirname(readme_path)):
    for name in sorted(os.listdir(os.path.dirname(readme_path))):
        if name == "README.md":
            continue
        candidate = os.path.join(os.path.dirname(readme_path), name, "index.md")
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
    f.write("- one detached render request\n")
    f.write("- one selected output frame for repo-doc inspection\n")
    f.write("- one copied render summary for downstream review\n\n")
    f.write("Typical contents per set:\n\n")
    f.write("- `request.json`\n")
    f.write("- `preview.bmp`\n")
    f.write("- optional `preview.png`\n")
    f.write("- `summary.json`\n")
    f.write("- `index.md`\n\n")
    f.write("These sets are intended to mirror one completed detached run in a stable\n")
    f.write("repo-doc form without keeping the full private run root exposed.\n\n")
    f.write("Current published sets:\n\n")
    for entry in entries:
        f.write(f"- `{entry}/`\n")
PY

echo "render review set published: $ROOT_DIR/docs/render_review_sets/$SET_ID"
