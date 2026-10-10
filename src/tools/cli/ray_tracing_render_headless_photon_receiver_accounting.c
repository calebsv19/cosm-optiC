#include "tools/ray_tracing_render_headless_internal.h"
#include "render/runtime_caustic_photon_receiver_accounting_3d.h"
#include <inttypes.h>
#include <stdio.h>

bool ray_tracing_headless_write_photon_receiver_accounting(
    const RayTracingAgentRenderRequest* request, int frame_index) {
    char path[PATH_MAX];
    RuntimePhotonReceiverAccounting3D accounting;
    RuntimePhotonReceiverAccounting3D_Snapshot(&accounting);
    if (snprintf(path, sizeof(path), "%s/photon_receiver_accounting_%04d.json",
                 request->output_root, frame_index) >= (int)sizeof(path)) return false;
    FILE* file = fopen(path, "w");
    if (!file) return false;
    bool ok = fprintf(file,
        "{\"schema\":\"photon_receiver_accounting_v1\","
        "\"domain\":\"live_query_calls_before_synthetic_grid\","
        "\"sum_semantics\":\"call_weighted_not_integrated_power_or_camera_pixel_radiance\","
        "\"legacy_grid_file\":\"photon_surface_queries_%04d.jsonl\","
        "\"live_query_capacity\":8192,\"receiver_capacity\":64,"
        "\"accounting_storage_bytes\":%zu,\"overflow_query_count\":%" PRIu64
        ",\"receivers\":[", frame_index, sizeof(accounting), accounting.overflowQueryCount) >= 0;
    for (uint64_t i = 0; i < accounting.bucketCount && ok; ++i) {
        const RuntimePhotonReceiverAccountingBucket3D* b = &accounting.buckets[i];
        ok = fprintf(file,
            "%s{\"scene_object_index\":%d,\"material_id\":%d,"
            "\"query_count\":%" PRIu64 ",\"positive_query_count\":%" PRIu64
            ",\"undersampled_positive_query_count\":%" PRIu64
            ",\"effective_sample_count_sum\":%" PRIu64
            ",\"receiver_reject_count\":%" PRIu64 ",\"normal_reject_count\":%" PRIu64
            ",\"irradiance_sum\":[%.17g,%.17g,%.17g],"
            "\"receiver_radiance_sum\":[%.17g,%.17g,%.17g],"
            "\"max_receiver_radiance\":%.17g,\"minimum_support_radius\":%.17g,"
            "\"maximum_support_radius\":%.17g}",
            i ? "," : "", b->sceneObjectIndex, b->materialId,
            b->queryCount, b->positiveQueryCount, b->undersampledPositiveQueryCount,
            b->effectiveSampleCountSum, b->receiverRejectCount, b->normalRejectCount,
            b->irradianceSum.x, b->irradianceSum.y, b->irradianceSum.z,
            b->radianceSum.x, b->radianceSum.y, b->radianceSum.z,
            b->maxRadiance, b->minimumSupportRadius, b->maximumSupportRadius) >= 0;
    }
    if (fprintf(file, "]}\n") < 0) ok = false;
    if (fclose(file) != 0) ok = false;
    return ok;
}
