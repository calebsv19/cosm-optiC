#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CLI="$ROOT_DIR/build/arm64/tools/cli/ray_tracing_material_preview_headless"

REQUEST=""
SET_ID=""
TITLE=""

while [ $# -gt 0 ]; do
  case "$1" in
    --request)
      REQUEST="$2"
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
    *)
      echo "usage: $0 --request /path/request.json --set-id material_set_id [--title 'Title']" >&2
      exit 2
      ;;
  esac
done

if [ -z "$REQUEST" ] || [ -z "$SET_ID" ]; then
  echo "usage: $0 --request /path/request.json --set-id material_set_id [--title 'Title']" >&2
  exit 2
fi

if [ -z "$TITLE" ]; then
  TITLE="$SET_ID"
fi

python3 - "$REQUEST" "$SET_ID" "$TITLE" "$ROOT_DIR" <<'PY'
import json
import os
import shutil
import subprocess
import sys

request_path, set_id, title, root_dir = sys.argv[1:5]
with open(request_path, "r", encoding="utf-8") as f:
    request = json.load(f)

output_path = request["output_path"]
summary_path = request.get("summary_path", "")
dest_dir = os.path.join(root_dir, "docs", "material_preview_sets", set_id)
os.makedirs(dest_dir, exist_ok=True)

subprocess.check_call([os.path.join(root_dir, "build", "arm64", "tools", "cli", "ray_tracing_material_preview_headless"),
                       "--request", request_path])

dest_bmp = os.path.join(dest_dir, "preview.bmp")
shutil.copy2(request_path, os.path.join(dest_dir, "request.json"))
shutil.copy2(output_path, dest_bmp)

png_written = False
dest_png = os.path.join(dest_dir, "preview.png")
if shutil.which("ffmpeg"):
    try:
        subprocess.check_call(["ffmpeg", "-y", "-i", dest_bmp, dest_png],
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
        png_written = True
    except subprocess.CalledProcessError:
        png_written = False

if summary_path:
    shutil.copy2(summary_path, os.path.join(dest_dir, "summary.json"))

index_path = os.path.join(dest_dir, "index.md")
variant_lines = []
if summary_path and os.path.exists(summary_path):
    try:
        with open(summary_path, "r", encoding="utf-8") as sf:
            summary = json.load(sf)
        for idx, variant in enumerate(summary.get("variants", []), start=1):
            label = variant.get("label", f"variant_{idx}")
            variant_lines.append(f"{idx}. `{label}`")
    except Exception:
        variant_lines = []

with open(index_path, "w", encoding="utf-8") as f:
    f.write(f"# {title}\n\n")
    f.write("Generated material preview set for `ray_tracing`.\n\n")
    f.write("- source request: `request.json`\n")
    f.write("- rendered preview: `preview.bmp`\n")
    if png_written:
        f.write("- web preview: `preview.png`\n")
    if summary_path:
        f.write("- effective summary: `summary.json`\n")
    if variant_lines:
        f.write("\n## Variant Order\n\n")
        f.write("Contact-sheet cells are ordered left-to-right, top-to-bottom.\n\n")
        for line in variant_lines:
            f.write(f"- {line}\n")
PY

echo "material preview set published: $ROOT_DIR/docs/material_preview_sets/$SET_ID"
