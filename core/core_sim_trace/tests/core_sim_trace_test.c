#include "core_sim_trace.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static int test_defaults(void) {
    CoreSimTraceFrameEmitOptions options;

    options.emit_frame_marker = false;
    options.emit_reason_markers = false;
    core_sim_trace_frame_emit_options_defaults(&options);
    return expect_true(options.emit_frame_marker, "frame marker default") &&
           expect_true(options.emit_reason_markers, "reason marker default");
}

static int test_emit_frame_record(void) {
    CoreTraceSession trace;
    CoreTraceConfig config = { 16u, 8u };
    CoreResult result;
    CoreSimFrameRecord record;
    const CoreTraceSampleF32 *samples = NULL;
    const CoreTraceMarker *markers = NULL;

    memset(&trace, 0, sizeof(trace));
    memset(&record, 0, sizeof(record));

    result = core_trace_session_init(&trace, &config);
    if (!expect_true(result.code == CORE_OK, "trace init")) return 0;

    record.frame_index = 7u;
    record.input_dt_seconds = 0.016;
    record.simulation_time_after_seconds = 1.25;
    record.simulation_time_advanced_seconds = 0.032;
    record.accumulator_remaining_seconds = 0.004;
    record.ticks_executed = 2u;
    record.passes_executed = 6u;
    record.reason_bits = CORE_SIM_FRAME_REASON_TICK_EXECUTED |
                         CORE_SIM_FRAME_REASON_RENDER_REQUESTED;
    record.status_name = "ok";

    result = core_sim_trace_emit_frame_record(&trace, &record, NULL);
    if (!expect_true(result.code == CORE_OK, "emit frame record")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_sample_count(&trace) == 7u, "sample count")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_marker_count(&trace) == 3u, "marker count")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    samples = core_trace_samples(&trace);
    markers = core_trace_markers(&trace);
    if (!expect_true(samples && markers, "trace storage")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(strcmp(samples[0].lane, CORE_SIM_TRACE_LANE_FRAME) == 0,
                     "frame lane")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(strcmp(samples[2].lane, CORE_SIM_TRACE_LANE_TICKS) == 0 &&
                         samples[2].value == 2.0f,
                     "ticks lane")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(strcmp(markers[0].label, "frame") == 0 &&
                         strcmp(markers[1].label, "tick_executed") == 0 &&
                         strcmp(markers[2].label, "render_requested") == 0,
                     "marker labels")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    core_trace_session_reset(&trace);
    return 1;
}

static int test_emit_without_markers(void) {
    CoreTraceSession trace;
    CoreTraceConfig config = { 16u, 8u };
    CoreSimTraceFrameEmitOptions options;
    CoreSimFrameRecord record;
    CoreResult result;

    memset(&trace, 0, sizeof(trace));
    memset(&record, 0, sizeof(record));
    result = core_trace_session_init(&trace, &config);
    if (!expect_true(result.code == CORE_OK, "trace init no markers")) return 0;

    options.emit_frame_marker = false;
    options.emit_reason_markers = false;
    record.reason_bits = CORE_SIM_FRAME_REASON_TICK_EXECUTED |
                         CORE_SIM_FRAME_REASON_RENDER_REQUESTED |
                         CORE_SIM_FRAME_REASON_MAX_TICK_CLAMP_HIT;

    result = core_sim_trace_emit_frame_record(&trace, &record, &options);
    if (!expect_true(result.code == CORE_OK, "emit no markers")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_sample_count(&trace) == 7u, "no marker sample count") ||
        !expect_true(core_trace_marker_count(&trace) == 0u, "no marker marker count")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    core_trace_session_reset(&trace);
    return 1;
}

int main(void) {
    if (!test_defaults()) return 1;
    if (!test_emit_frame_record()) return 1;
    if (!test_emit_without_markers()) return 1;

    puts("core_sim_trace_test: ok");
    return 0;
}
