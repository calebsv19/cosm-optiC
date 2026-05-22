#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PUBLISH_SCRIPT="$ROOT_DIR/tools/publish_render_outputs.sh"
DEFAULT_RUNS_ROOT="$ROOT_DIR/../_private_workspace_artifacts/agent_runs/ray_tracing"

RUN_ROOT=""
RUNS_ROOT="$DEFAULT_RUNS_ROOT"
SET_ID=""
TITLE=""
FRAME_NAME=""
MODE="visualizer"
DROP_ID=""
JOB_TYPE="render-review"
SUMMARY_TEXT=""
INCLUDE_ALL_FRAMES=0
STAGE_ONLY=0

usage() {
  echo "usage: $0 [--run-root /abs/run/root] [--runs-root /abs/runs/root] [--set-id id] [--title 'Title'] [--frame frame_0005.bmp] [--mode local|visualizer|both] [--drop-id run_id] [--job-type render-review] [--summary 'summary'] [--include-all-frames] [--stage-only]" >&2
}

while [ $# -gt 0 ]; do
  case "$1" in
    --run-root)
      RUN_ROOT="$2"
      shift 2
      ;;
    --runs-root)
      RUNS_ROOT="$2"
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
    --mode)
      MODE="$2"
      shift 2
      ;;
    --drop-id)
      DROP_ID="$2"
      shift 2
      ;;
    --job-type)
      JOB_TYPE="$2"
      shift 2
      ;;
    --summary)
      SUMMARY_TEXT="$2"
      shift 2
      ;;
    --include-all-frames)
      INCLUDE_ALL_FRAMES=1
      shift 1
      ;;
    --stage-only)
      STAGE_ONLY=1
      shift 1
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [ -z "$RUN_ROOT" ]; then
  RUN_ROOT="$(python3 - "$RUNS_ROOT" <<'PY'
import os
import sys

runs_root = sys.argv[1]
best = None
best_mtime = None
if not os.path.isdir(runs_root):
    raise SystemExit(2)
for name in os.listdir(runs_root):
    candidate = os.path.join(runs_root, name)
    frame_dir = os.path.join(candidate, "ray_tracing", "output", "frames")
    if not os.path.isdir(frame_dir):
        continue
    try:
        mtime = os.path.getmtime(frame_dir)
    except OSError:
        continue
    if best is None or mtime > best_mtime:
        best = candidate
        best_mtime = mtime
if best is None:
    raise SystemExit(3)
print(best)
PY
)"
fi

if [ ! -d "$RUN_ROOT" ]; then
  echo "run root not found: $RUN_ROOT" >&2
  exit 2
fi

FRAME_DIR="$RUN_ROOT/ray_tracing/output/frames"
if [ ! -d "$FRAME_DIR" ]; then
  echo "frame directory not found: $FRAME_DIR" >&2
  exit 2
fi

if [ -z "$FRAME_NAME" ]; then
  FRAME_NAME="$(python3 - "$FRAME_DIR" <<'PY'
import os
import sys

frame_dir = sys.argv[1]
names = []
for name in os.listdir(frame_dir):
    path = os.path.join(frame_dir, name)
    if not os.path.isfile(path):
        continue
    lower = name.lower()
    if lower.endswith((".bmp", ".png", ".jpg", ".jpeg")):
        names.append(name)
if not names:
    raise SystemExit(2)
names.sort()
print(names[-1])
PY
)"
fi

if [ -z "$SET_ID" ]; then
  SET_ID="$(basename "$RUN_ROOT")"
fi

if [ -z "$TITLE" ]; then
  TITLE="$(python3 - "$SET_ID" <<'PY'
import sys
value = sys.argv[1].replace("_", " ").replace("-", " ").strip()
print(" ".join(word.capitalize() for word in value.split()))
PY
)"
fi

if [ -z "$SUMMARY_TEXT" ]; then
  SUMMARY_TEXT="$TITLE"
fi

set -- \
  --run-root "$RUN_ROOT" \
  --set-id "$SET_ID" \
  --title "$TITLE" \
  --frame "$FRAME_NAME" \
  --mode "$MODE" \
  --job-type "$JOB_TYPE" \
  --summary "$SUMMARY_TEXT"

if [ -n "$DROP_ID" ]; then
  set -- "$@" --drop-id "$DROP_ID"
fi
if [ "$INCLUDE_ALL_FRAMES" -eq 1 ]; then
  set -- "$@" --include-all-frames
fi
if [ "$STAGE_ONLY" -eq 1 ]; then
  set -- "$@" --stage-only
fi

"$PUBLISH_SCRIPT" "$@"
