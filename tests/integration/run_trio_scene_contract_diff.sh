#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
"$ROOT/shared/assets/scenes/trio_contract/run_scene_contract_diff_smoke.sh"
