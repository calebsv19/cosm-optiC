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

ray_tracing_workspace_root() {
  local root_dir="${1:-$(ray_tracing_root_dir)}"
  if [[ -d "$root_dir/../skills" ]]; then
    cd "$root_dir/.." && pwd
    return 0
  fi
  if [[ -d "$root_dir/../../skills" ]]; then
    cd "$root_dir/../.." && pwd
    return 0
  fi
  cd "$root_dir/.." && pwd
}

ray_tracing_tool_path() {
  local tool_name="$1"
  local root_dir="${2:-$(ray_tracing_root_dir)}"
  local arch
  arch="$(uname -m)"
  if [[ "${BUILD_DIR_BASE:-build}" != "build" ]]; then
    python3 "$root_dir/tools/preparation_paths.py" --repository "$root_dir" \
      --build-root "$BUILD_DIR_BASE" --tool "$tool_name" --architecture "$arch"
    return $?
  fi
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
  local selected="${RAY_TRACING_TEST_WORK_ROOT:-${BUILD_DIR_BASE:-build}/agent_runs/ray_tracing}"
  if [[ "${BUILD_DIR_BASE:-build}" != "build" ]]; then
    python3 "$root_dir/tools/preparation_paths.py" --repository "$root_dir" \
      --build-root "$BUILD_DIR_BASE" --test-root "$selected" --fixture-label "$label"
    return $?
  fi
  if [[ "$selected" != /* ]]; then selected="$root_dir/$selected"; fi
  printf '%s\n' "$selected/$label"
}

ray_tracing_test_reset_work_root() {
  local label="$1"
  local root_dir="${2:-$(ray_tracing_root_dir)}"
  local work_root
  work_root="$(ray_tracing_test_work_root "$label" "$root_dir")"
  if [[ "${BUILD_DIR_BASE:-build}" != "build" ]]; then
    if [[ -e "$work_root" || -L "$work_root" ]]; then
      echo "Refusing occupied preparation fixture root: $work_root" >&2
      return 1
    fi
  else
    rm -rf "$work_root"
  fi
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
  local workspace_root
  workspace_root="$(ray_tracing_workspace_root "$root_dir")"
  printf '%s\n' "$workspace_root/_private_workspace_artifacts/codework_visualizer_runs/$drop_id"
}
