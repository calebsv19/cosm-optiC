#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"
RAY_TRACING_ROOT="$(ray_tracing_root_dir)"
WORKSPACE_ROOT="$(ray_tracing_workspace_root "$RAY_TRACING_ROOT")"
"$WORKSPACE_ROOT/shared/assets/scenes/trio_contract/run_scene_contract_diff_smoke.sh"
