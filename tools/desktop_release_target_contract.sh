#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  tools/desktop_release_target_contract.sh get <key>

Keys:
  host_arch
  target_os
  target_arch
  target_variant
  target_triple
  release_platform
  release_arch
  homebrew_prefix
  alt_homebrew_prefix
USAGE
}

normalize_arch() {
  case "$1" in
    arm64|aarch64)
      printf 'arm64\n'
      ;;
    x86_64|amd64)
      printf 'x86_64\n'
      ;;
    *)
      printf '%s\n' "$1"
      ;;
  esac
}

resolve_host_arch() {
  normalize_arch "$(uname -m)"
}

resolve_target_os() {
  local value="${TARGET_OS:-macOS}"
  case "$value" in
    macOS|MacOS|darwin|Darwin)
      printf 'macOS\n'
      ;;
    *)
      echo "unsupported TARGET_OS: $value" >&2
      exit 2
      ;;
  esac
}

resolve_target_arch() {
  local value="${TARGET_ARCH:-}"
  if [[ -z "$value" ]]; then
    value="$(resolve_host_arch)"
  fi
  value="$(normalize_arch "$value")"
  case "$value" in
    arm64|x86_64)
      printf '%s\n' "$value"
      ;;
    *)
      echo "unsupported TARGET_ARCH: $value" >&2
      exit 2
      ;;
  esac
}

resolve_target_variant() {
  printf '%s\n' "${TARGET_VARIANT:-desktop-app}"
}

resolve_target_triple() {
  printf '%s-%s\n' "$(resolve_target_os)" "$(resolve_target_arch)"
}

resolve_homebrew_prefix() {
  case "$(resolve_target_arch)" in
    arm64)
      printf '/opt/homebrew\n'
      ;;
    x86_64)
      printf '/usr/local\n'
      ;;
  esac
}

resolve_alt_homebrew_prefix() {
  case "$(resolve_target_arch)" in
    arm64)
      printf '/usr/local\n'
      ;;
    x86_64)
      printf '/opt/homebrew\n'
      ;;
  esac
}

if [[ $# -ne 2 || "$1" != "get" ]]; then
  usage >&2
  exit 2
fi

case "$2" in
  host_arch)
    resolve_host_arch
    ;;
  target_os)
    resolve_target_os
    ;;
  target_arch)
    resolve_target_arch
    ;;
  target_variant)
    resolve_target_variant
    ;;
  target_triple)
    resolve_target_triple
    ;;
  release_platform)
    resolve_target_os
    ;;
  release_arch)
    resolve_target_arch
    ;;
  homebrew_prefix)
    resolve_homebrew_prefix
    ;;
  alt_homebrew_prefix)
    resolve_alt_homebrew_prefix
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
