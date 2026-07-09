#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

ROOT_DIR="$(ray_tracing_root_dir)"
WORK_ROOT="$(ray_tracing_test_reset_work_root release_contract_redaction "$ROOT_DIR")"
OUT_PATH="$WORK_ROOT/release_contract.out"
ERR_PATH="$WORK_ROOT/release_contract.err"

SENTINEL_SIGNING="R5S3_SENTINEL_SIGNING_SECRET"
SENTINEL_NOTARY="R5S3_SENTINEL_NOTARY_SECRET"
SENTINEL_TEAM="R5S3_SENTINEL_TEAM_SECRET"

(
  cd "$ROOT_DIR"
  APPLE_SIGN_IDENTITY="$SENTINEL_SIGNING" \
    APPLE_NOTARY_PROFILE="$SENTINEL_NOTARY" \
    APPLE_TEAM_ID="$SENTINEL_TEAM" \
    make release-contract >"$OUT_PATH" 2>"$ERR_PATH"
)

if grep -F "$SENTINEL_SIGNING" "$OUT_PATH" "$ERR_PATH"; then
  echo "release-contract leaked signing identity sentinel" >&2
  exit 1
fi
if grep -F "$SENTINEL_NOTARY" "$OUT_PATH" "$ERR_PATH"; then
  echo "release-contract leaked notary profile sentinel" >&2
  exit 1
fi
if grep -F "$SENTINEL_TEAM" "$OUT_PATH" "$ERR_PATH"; then
  echo "release-contract leaked team id sentinel" >&2
  exit 1
fi

grep -q "signing_identity_set: yes" "$OUT_PATH"
grep -q "notary_profile_set: yes" "$OUT_PATH"
grep -q "team_id_set: yes" "$OUT_PATH"

echo "ray tracing release-contract redaction passed"
