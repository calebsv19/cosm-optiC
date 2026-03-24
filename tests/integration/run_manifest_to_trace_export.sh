#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

TRACE_PATH="$TMP_DIR/ray_trace_tool_smoke.pack"
CHECKER_C="$TMP_DIR/check_trace_contract.c"
CHECKER_BIN="$TMP_DIR/check_trace_contract"
SOURCE_PATH="$ROOT_DIR/../shared/core/core_pack/tests/fixtures/physics_v1_sample.pack"

"$ROOT_DIR/ray_trace_tool" "$SOURCE_PATH" "$TRACE_PATH" 6
if [[ ! -f "$TRACE_PATH" ]]; then
    echo "manifest trace export test failed: missing trace pack at $TRACE_PATH"
    exit 10
fi

cat > "$CHECKER_C" <<'C_EOF'
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core_trace.h"

static bool lane_present(const CoreTraceSession *s, const char *lane) {
    size_t count = core_trace_sample_count(s);
    const CoreTraceSampleF32 *samples = core_trace_samples(s);
    if (!samples) return false;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(samples[i].lane, lane) == 0) return true;
    }
    return false;
}

static bool marker_present(const CoreTraceSession *s, const char *lane, const char *label) {
    size_t count = core_trace_marker_count(s);
    const CoreTraceMarker *markers = core_trace_markers(s);
    if (!markers) return false;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(markers[i].lane, lane) == 0 && strcmp(markers[i].label, label) == 0) return true;
    }
    return false;
}

int main(int argc, char **argv) {
    CoreTraceSession session;
    CoreResult r;
    if (argc != 2) return 2;

    memset(&session, 0, sizeof(session));
    r = core_trace_import_pack(argv[1], &session);
    if (r.code != CORE_OK) {
        fprintf(stderr, "trace contract check failed: import error (%s)\n", r.message);
        return 3;
    }

    if (core_trace_sample_count(&session) != 4u || core_trace_marker_count(&session) != 2u) {
        fprintf(stderr,
                "trace contract check failed: unexpected counts (samples=%zu markers=%zu)\n",
                core_trace_sample_count(&session),
                core_trace_marker_count(&session));
        core_trace_session_reset(&session);
        return 4;
    }

    if (!lane_present(&session, "frame_dt") ||
        !lane_present(&session, "frame_index") ||
        !lane_present(&session, "grid_cells") ||
        !lane_present(&session, "bounce_depth")) {
        fprintf(stderr, "trace contract check failed: missing canonical sample lane(s)\n");
        core_trace_session_reset(&session);
        return 5;
    }

    if (!marker_present(&session, "events", "trace_start") ||
        !marker_present(&session, "events", "trace_end")) {
        fprintf(stderr, "trace contract check failed: missing canonical marker(s)\n");
        core_trace_session_reset(&session);
        return 6;
    }

    core_trace_session_reset(&session);
    puts("manifest trace export test passed.");
    return 0;
}
C_EOF

cc -std=c11 -Wall -Wextra -Wpedantic -g \
   -I"$ROOT_DIR/../shared/core/core_trace/include" \
   -I"$ROOT_DIR/../shared/core/core_pack/include" \
   -I"$ROOT_DIR/../shared/core/core_io/include" \
   -I"$ROOT_DIR/../shared/core/core_base/include" \
   "$CHECKER_C" \
   "$ROOT_DIR/../shared/core/core_trace/src/core_trace.c" \
   "$ROOT_DIR/../shared/core/core_pack/src/core_pack.c" \
   "$ROOT_DIR/../shared/core/core_io/src/core_io.c" \
   "$ROOT_DIR/../shared/core/core_base/src/core_base.c" \
   -o "$CHECKER_BIN" -lm

"$CHECKER_BIN" "$TRACE_PATH"
