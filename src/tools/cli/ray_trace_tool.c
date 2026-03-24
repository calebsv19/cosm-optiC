#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_trace.h"
#include "import/fluid_import.h"

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s <source_manifest_or_bundle_or_frame> <trace_pack_path> [bounce_depth]\n",
            argv0);
}

static float to_f32_u64(unsigned long long value) {
    if (value > 16777216ull) {
        return 16777216.0f;
    }
    return (float)value;
}

int main(int argc, char **argv) {
    const char *source_path = NULL;
    const char *trace_pack_path = NULL;
    int bounce_depth = 4;
    FluidManifest manifest;
    CoreTraceSession session;
    CoreTraceConfig cfg;
    CoreResult r;
    double start_time = 0.0;
    double end_time = 0.0;
    unsigned long long grid_cells = 0ull;

    if (argc < 3 || argc > 4) {
        usage(argv[0]);
        return 1;
    }

    source_path = argv[1];
    trace_pack_path = argv[2];
    if (argc == 4) {
        bounce_depth = atoi(argv[3]);
        if (bounce_depth < 1) bounce_depth = 1;
    }

    memset(&manifest, 0, sizeof(manifest));
    if (!fluid_manifest_load(source_path, &manifest)) {
        fprintf(stderr, "ray_trace_tool: failed to load source: %s\n", source_path);
        return 1;
    }
    if (manifest.count == 0u) {
        fluid_manifest_free(&manifest);
        fprintf(stderr, "ray_trace_tool: source resolved to zero frames: %s\n", source_path);
        return 1;
    }

    memset(&session, 0, sizeof(session));
    cfg.sample_capacity = manifest.count * 4u + 8u;
    cfg.marker_capacity = 4u;
    r = core_trace_session_init(&session, &cfg);
    if (r.code != CORE_OK) {
        fluid_manifest_free(&manifest);
        fprintf(stderr, "ray_trace_tool: failed to init trace session (%s)\n", r.message);
        return 1;
    }

    start_time = manifest.meta[0].time_seconds;
    end_time = manifest.meta[manifest.count - 1u].time_seconds;
    grid_cells = (unsigned long long)manifest.grid_w * (unsigned long long)manifest.grid_h;

    r = core_trace_emit_marker(&session, "events", start_time, "trace_start");
    if (r.code != CORE_OK) {
        fprintf(stderr, "ray_trace_tool: emit trace_start failed (%s)\n", r.message);
        core_trace_session_reset(&session);
        fluid_manifest_free(&manifest);
        return 1;
    }

    for (size_t i = 0; i < manifest.count; ++i) {
        const FluidFrameMeta *m = &manifest.meta[i];

        r = core_trace_emit_sample_f32(&session, "frame_dt", m->time_seconds, (float)m->dt_seconds);
        if (r.code != CORE_OK) break;

        r = core_trace_emit_sample_f32(&session,
                                       "frame_index",
                                       m->time_seconds,
                                       to_f32_u64((unsigned long long)m->frame_index));
        if (r.code != CORE_OK) break;

        r = core_trace_emit_sample_f32(&session, "grid_cells", m->time_seconds, to_f32_u64(grid_cells));
        if (r.code != CORE_OK) break;

        r = core_trace_emit_sample_f32(&session, "bounce_depth", m->time_seconds, (float)bounce_depth);
        if (r.code != CORE_OK) break;
    }
    if (r.code != CORE_OK) {
        fprintf(stderr, "ray_trace_tool: emit sample failed (%s)\n", r.message);
        core_trace_session_reset(&session);
        fluid_manifest_free(&manifest);
        return 1;
    }

    r = core_trace_emit_marker(&session, "events", end_time, "trace_end");
    if (r.code != CORE_OK) {
        fprintf(stderr, "ray_trace_tool: emit trace_end failed (%s)\n", r.message);
        core_trace_session_reset(&session);
        fluid_manifest_free(&manifest);
        return 1;
    }

    r = core_trace_finalize(&session);
    if (r.code != CORE_OK) {
        fprintf(stderr, "ray_trace_tool: finalize failed (%s)\n", r.message);
        core_trace_session_reset(&session);
        fluid_manifest_free(&manifest);
        return 1;
    }

    r = core_trace_export_pack(&session, trace_pack_path);
    if (r.code != CORE_OK) {
        fprintf(stderr, "ray_trace_tool: export failed (%s)\n", r.message);
        core_trace_session_reset(&session);
        fluid_manifest_free(&manifest);
        return 1;
    }

    printf("wrote trace pack: %s\n", trace_pack_path);
    printf("source=%s frames=%zu bounce_depth=%d\n", source_path, manifest.count, bounce_depth);

    core_trace_session_reset(&session);
    fluid_manifest_free(&manifest);
    return 0;
}
