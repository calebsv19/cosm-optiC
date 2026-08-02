#!/bin/sh

rt_publish_workspace_root() {
  program_root="$1"
  candidate="${CODEWORK_ROOT:-$program_root/..}"
  if [ ! -d "$candidate/skills" ] && [ -d "$program_root/../../skills" ]; then
    candidate="$program_root/../.."
  fi
  cd "$candidate" >/dev/null 2>&1 || return 1
  pwd -P
}

rt_publish_die() {
  echo "$1" >&2
  exit 2
}

rt_publish_validate_segment() {
  label="$1"
  value="$2"
  if [ -z "$value" ]; then
    rt_publish_die "$label is required"
  fi
  case "$value" in
    .|..|-*|*/*|*\\*|*[!ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-]*)
      rt_publish_die "invalid $label: expected one path segment using A-Z, a-z, 0-9, dot, underscore, or dash"
      ;;
  esac
}

rt_publish_validate_optional_segment() {
  label="$1"
  value="$2"
  if [ -n "$value" ]; then
    rt_publish_validate_segment "$label" "$value"
  fi
}

rt_publish_require_absolute_existing_dir() {
  label="$1"
  value="$2"
  if [ -z "$value" ]; then
    rt_publish_die "$label is required"
  fi
  case "$value" in
    /*)
      ;;
    *)
      rt_publish_die "$label must be an absolute existing directory: $value"
      ;;
  esac
  if [ ! -d "$value" ]; then
    rt_publish_die "$label must be an absolute existing directory: $value"
  fi
}

rt_publish_canonical_dir() {
  value="$1"
  cd "$value" >/dev/null 2>&1 || rt_publish_die "directory not readable: $value"
  pwd -P
}
