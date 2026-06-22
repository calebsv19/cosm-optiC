#!/usr/bin/env bash

ray_tracing_integration_dir() {
  local common_dir
  common_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  cd "$common_dir/.." && pwd
}

ray_tracing_root_dir() {
  local integration_dir
  integration_dir="$(ray_tracing_integration_dir)"
  cd "$integration_dir/../.." && pwd
}

ray_tracing_tool_path() {
  local tool_name="$1"
  local root_dir="${2:-$(ray_tracing_root_dir)}"
  local arch
  arch="$(uname -m)"
  local fallback="$root_dir/build/$arch/tools/cli/$tool_name"
  local toolchain="$root_dir/build/toolchains/clang/$arch/tools/cli/$tool_name"
  if [[ -x "$toolchain" ]]; then
    printf '%s\n' "$toolchain"
    return 0
  fi
  printf '%s\n' "$fallback"
}

ray_tracing_test_work_root() {
  local label="$1"
  local root_dir="${2:-$(ray_tracing_root_dir)}"
  printf '%s\n' "$root_dir/build/agent_runs/ray_tracing/$label"
}

ray_tracing_test_reset_work_root() {
  local label="$1"
  local root_dir="${2:-$(ray_tracing_root_dir)}"
  local work_root
  work_root="$(ray_tracing_test_work_root "$label" "$root_dir")"
  rm -rf "$work_root"
  mkdir -p "$work_root"
  printf '%s\n' "$work_root"
}

ray_tracing_test_diagnostics_dir() {
  local label="$1"
  local root_dir="${2:-$(ray_tracing_root_dir)}"
  local work_root
  work_root="$(ray_tracing_test_work_root "$label" "$root_dir")"
  mkdir -p "$work_root/diagnostics"
  printf '%s\n' "$work_root/diagnostics"
}

ray_tracing_visualizer_drop_root() {
  local drop_id="$1"
  local root_dir="${2:-$(ray_tracing_root_dir)}"
  printf '%s\n' "$root_dir/../_private_workspace_artifacts/codework_visualizer_runs/$drop_id"
}
