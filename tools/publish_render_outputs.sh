#!/bin/sh
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
STAGE_SCRIPT="$ROOT_DIR/../skills/codework-visualizer-drop/scripts/stage_visualizer_run.py"
UPLOAD_SCRIPT="$ROOT_DIR/../skills/codework-visualizer-drop/scripts/upload_visualizer_drop.sh"
LOCAL_REVIEW_SCRIPT="$ROOT_DIR/tools/publish_render_review_set.sh"

RUN_ROOT=""
SET_ID=""
TITLE=""
FRAME_NAME="frame_0000.bmp"
MODE="visualizer"
DROP_ID=""
JOB_TYPE="render-review"
SUMMARY_TEXT=""
INCLUDE_ALL_FRAMES=0
STAGE_ONLY=0

usage() {
  echo "usage: $0 --run-root /abs/run/root --set-id review_set_id [--title 'Title'] [--frame frame_0000.bmp] [--mode local|visualizer|both] [--drop-id run_id] [--job-type render-review] [--summary 'summary'] [--include-all-frames] [--stage-only]" >&2
}

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

if [ -z "$RUN_ROOT" ] || [ -z "$SET_ID" ]; then
  usage
  exit 2
fi

case "$MODE" in
  local|visualizer|both)
    ;;
  *)
    echo "invalid --mode: $MODE" >&2
    exit 2
    ;;
esac

if [ -z "$TITLE" ]; then
  TITLE="$SET_ID"
fi

FRAME_PATH="$RUN_ROOT/ray_tracing/output/frames/$FRAME_NAME"
REQUEST_PATH="$RUN_ROOT/ray_request.json"
RENDER_SUMMARY_PATH="$RUN_ROOT/ray_tracing/render_summary.json"
PUBLISH_LOG_PATH="$RUN_ROOT/ray_tracing/publish_run.log"

if [ ! -f "$FRAME_PATH" ]; then
  echo "frame not found: $FRAME_PATH" >&2
  exit 2
fi
if [ ! -f "$REQUEST_PATH" ]; then
  echo "request not found: $REQUEST_PATH" >&2
  exit 2
fi

if [ "$MODE" = "local" ] || [ "$MODE" = "both" ]; then
  "$LOCAL_REVIEW_SCRIPT" \
    --run-root "$RUN_ROOT" \
    --set-id "$SET_ID" \
    --title "$TITLE" \
    --frame "$FRAME_NAME"
fi

if [ "$MODE" = "visualizer" ] || [ "$MODE" = "both" ]; then
  if [ -z "$DROP_ID" ]; then
    DROP_ID="$(python3 - "$SET_ID" "$JOB_TYPE" <<'PY'
import re
import sys
from datetime import datetime, timezone

set_id, job_type = sys.argv[1:3]
stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
nonce = re.sub(r"[^a-z0-9]", "", set_id.lower()) or "run"
print(f"ray-tracing--{job_type}--{stamp}--{nonce}")
PY
)"
  fi

  if [ -z "$SUMMARY_TEXT" ]; then
    SUMMARY_TEXT="$TITLE"
  fi

  cat > "$PUBLISH_LOG_PATH" <<EOF
publish_render_outputs
run_root=$RUN_ROOT
set_id=$SET_ID
title=$TITLE
frame=$FRAME_NAME
mode=$MODE
drop_id=$DROP_ID
job_type=$JOB_TYPE
render_summary_path=$RENDER_SUMMARY_PATH
request_path=$REQUEST_PATH
EOF

  set -- \
    --drop-id "$DROP_ID" \
    --program ray-tracing \
    --job-type "$JOB_TYPE" \
    --summary "$SUMMARY_TEXT" \
    --preview-source "$FRAME_PATH" \
    --log-source "$PUBLISH_LOG_PATH" \
    --primary-output-source "$FRAME_PATH" \
    --primary-output-relpath "outputs/$FRAME_NAME" \
    --output "$REQUEST_PATH|inputs/request.json|request|application/json" \
    --convert-bmp-to-png \
    --overwrite \
    --write-ready

  if [ -f "$RENDER_SUMMARY_PATH" ]; then
    set -- "$@" --output "$RENDER_SUMMARY_PATH|outputs/render_summary.json|summary|application/json"
  fi

  if [ "$INCLUDE_ALL_FRAMES" -eq 1 ]; then
    for candidate in "$RUN_ROOT"/ray_tracing/output/frames/*; do
      if [ ! -f "$candidate" ]; then
        continue
      fi
      base_name="$(basename "$candidate")"
      set -- "$@" --output "$candidate|outputs/frames/$base_name|frame|image/bmp"
    done
  fi

  python3 "$STAGE_SCRIPT" "$@"

  if [ "$STAGE_ONLY" -eq 0 ]; then
    "$UPLOAD_SCRIPT" \
      "$ROOT_DIR/../_private_workspace_artifacts/codework_visualizer_runs/$DROP_ID" \
      "$DROP_ID"
  fi

  echo "render outputs published to visualizer lane"
  echo "drop_id=$DROP_ID"
  echo "local_drop_dir=$ROOT_DIR/../_private_workspace_artifacts/codework_visualizer_runs/$DROP_ID"
  if [ "$STAGE_ONLY" -eq 1 ]; then
    echo "remote_upload=skipped"
  fi
fi
