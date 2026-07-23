#include "tools/ray_tracing_render_headless_internal.h"

#include <inttypes.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

#include "render/runtime_caustic_photon_direct_consumer_3d.h"
#include "render/runtime_native_3d_render_photon_prepare.h"

static void sample_surface_reconstruction_grid(void) {
    enum { GRID_SIZE = 80 };
    RuntimeCausticPhotonMapRecord3D first;
    RuntimeCausticPhotonMapRecord3D record;
    double min_x = DBL_MAX;
    double min_y = DBL_MAX;
    double max_x = -DBL_MAX;
    double max_y = -DBL_MAX;
    const uint64_t count =
        RuntimeNative3DRender_CausticPhotonSurfaceRecordCount();

    if (count == 0u ||
        !RuntimeNative3DRender_CausticPhotonSurfaceRecordAt(0u, &first)) {
        return;
    }
    RuntimeCausticPhotonDirectConsumer3D_ResetSurfaceDiagnostics();
    for (uint64_t i = 0u; i < count; ++i) {
        if (!RuntimeNative3DRender_CausticPhotonSurfaceRecordAt(i, &record)) {
            return;
        }
        if (record.position.x < min_x) min_x = record.position.x;
        if (record.position.x > max_x) max_x = record.position.x;
        if (record.position.y < min_y) min_y = record.position.y;
        if (record.position.y > max_y) max_y = record.position.y;
    }
    if (!(max_x > min_x) || !(max_y > min_y)) return;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            HitInfo3D hit;
            RuntimeCausticPhotonMapQueryResult3D query;
            Vec3 radiance;
            memset(&hit, 0, sizeof(hit));
            hit.position = vec3(
                min_x + (max_x - min_x) * ((double)x + 0.5) / GRID_SIZE,
                min_y + (max_y - min_y) * ((double)y + 0.5) / GRID_SIZE,
                first.position.z);
            hit.normal = first.normal;
            hit.geometricNormal = first.normal;
            hit.shadingNormal = first.normal;
            hit.sceneObjectIndex = first.sceneObjectIndex;
            hit.primitiveIndex = first.primitiveIndex;
            hit.triangleIndex = first.triangleIndex;
            hit.localTriangleIndex = first.triangleIndex;
            (void)RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                &hit, &radiance, &query);
        }
    }
}

static bool write_surface_records(FILE* file) {
    const uint64_t count =
        RuntimeNative3DRender_CausticPhotonSurfaceRecordCount();
    for (uint64_t i = 0u; i < count; ++i) {
        RuntimeCausticPhotonMapRecord3D record;
        if (!RuntimeNative3DRender_CausticPhotonSurfaceRecordAt(i, &record)) {
            return false;
        }
        if (fprintf(
                file,
                "{\"index\":%" PRIu64 ",\"photon_id\":%" PRIu64
                ",\"depth\":%u,\"position\":[%.17g,%.17g,%.17g]"
                ",\"normal\":[%.17g,%.17g,%.17g]"
                ",\"incident_direction\":[%.17g,%.17g,%.17g]"
                ",\"flux\":[%.17g,%.17g,%.17g],\"path_pdf\":%.17g"
                ",\"query_radius\":%.17g,\"support_radius\":%.17g"
                ",\"support_neighbors\":%" PRIu64
                ",\"support_adaptive\":%s,\"support_prepared\":%s"
                ",\"scene_object_index\":%d,\"primitive_index\":%d"
                ",\"triangle_index\":%d,\"material_id\":%d}\n",
                i,
                record.photonId,
                record.depth,
                record.position.x,
                record.position.y,
                record.position.z,
                record.normal.x,
                record.normal.y,
                record.normal.z,
                record.incidentDirection.x,
                record.incidentDirection.y,
                record.incidentDirection.z,
                record.flux.x,
                record.flux.y,
                record.flux.z,
                record.pathPdf,
                record.queryRadius,
                record.sampleCenteredSupportRadius,
                record.sampleCenteredSupportNeighborCount,
                record.sampleCenteredSupportAdaptive ? "true" : "false",
                record.sampleCenteredSupportPrepared ? "true" : "false",
                record.sceneObjectIndex,
                record.primitiveIndex,
                record.triangleIndex,
                record.materialId) < 0) {
            return false;
        }
    }
    return !ferror(file);
}

static bool write_surface_queries(FILE* file) {
    const uint64_t count =
        RuntimeCausticPhotonDirectConsumer3D_SurfaceDiagnosticCount();
    for (uint64_t i = 0u; i < count; ++i) {
        RuntimeCausticPhotonSurfaceDiagnosticSample3D sample;
        if (!RuntimeCausticPhotonDirectConsumer3D_SurfaceDiagnosticAt(
                i, &sample)) {
            return false;
        }
        if (fprintf(
                file,
                "{\"index\":%" PRIu64
                ",\"position\":[%.17g,%.17g,%.17g]"
                ",\"support_radius\":%.17g,\"density_estimate\":%.17g"
                ",\"nearest_distance\":%.17g"
                ",\"nearest_contribution_distance\":%.17g"
                ",\"farthest_contribution_distance\":%.17g"
                ",\"candidate_count\":%" PRIu64
                ",\"effective_sample_count\":%" PRIu64
                ",\"neighbor_limit\":%" PRIu64
                ",\"radius_reject_count\":%" PRIu64
                ",\"normal_reject_count\":%" PRIu64
                ",\"incident_hemisphere_reject_count\":%" PRIu64
                ",\"receiver_reject_count\":%" PRIu64
                ",\"receiver_object_reject_count\":%" PRIu64
                ",\"receiver_material_reject_count\":%" PRIu64
                ",\"receiver_exact_triangle_reject_count\":%" PRIu64
                ",\"physical_flux\":[%.17g,%.17g,%.17g]"
                ",\"query_hit\":%s,\"undersampled\":%s}\n",
                i,
                sample.position.x,
                sample.position.y,
                sample.position.z,
                sample.supportRadius,
                sample.densityEstimate,
                sample.nearestDistance,
                sample.nearestContributionDistance,
                sample.farthestContributionDistance,
                sample.candidateCount,
                sample.effectiveSampleCount,
                sample.neighborLimit,
                sample.radiusRejectCount,
                sample.normalRejectCount,
                sample.incidentHemisphereRejectCount,
                sample.receiverRejectCount,
                sample.receiverObjectRejectCount,
                sample.receiverMaterialRejectCount,
                sample.receiverExactTriangleRejectCount,
                sample.physicalFlux.x,
                sample.physicalFlux.y,
                sample.physicalFlux.z,
                sample.queryHit ? "true" : "false",
                sample.undersampled ? "true" : "false") < 0) {
            return false;
        }
    }
    return !ferror(file);
}

bool ray_tracing_headless_write_photon_surface_diagnostics(
    const RayTracingAgentRenderRequest* request,
    int frame_index) {
    char records_path[PATH_MAX];
    char queries_path[PATH_MAX];
    FILE* records = NULL;
    FILE* queries = NULL;
    bool ok = false;

    if (!request || !request->caustic_photon_render_prep_population_enabled ||
        !request->caustic_photon_surface_diagnostics_enabled) {
        return true;
    }
    if (snprintf(records_path,
                 sizeof(records_path),
                 "%s/photon_surface_records_%04d.jsonl",
                 request->output_root,
                 frame_index) >= (int)sizeof(records_path) ||
        snprintf(queries_path,
                 sizeof(queries_path),
                 "%s/photon_surface_queries_%04d.jsonl",
                 request->output_root,
                 frame_index) >= (int)sizeof(queries_path)) {
        return false;
    }
    records = fopen(records_path, "w");
    queries = fopen(queries_path, "w");
    if (!records || !queries) goto cleanup;
    sample_surface_reconstruction_grid();
    ok = write_surface_records(records) && write_surface_queries(queries);

cleanup:
    if (records && fclose(records) != 0) ok = false;
    if (queries && fclose(queries) != 0) ok = false;
    return ok;
}
