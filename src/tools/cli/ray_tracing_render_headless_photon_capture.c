#include "tools/ray_tracing_render_headless_photon_capture.h"
#include "tools/ray_tracing_render_headless_internal.h"
#include "tools/ray_tracing_headless_photon_termination.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <json-c/json.h>

#include "config/config_file_io.h"
#include "render/runtime_caustic_photon_path_scheduler_3d.h"
#include "render/runtime_caustic_photon_direct_consumer_3d.h"
#include "render/runtime_native_3d_render_photon_prepare.h"

enum { MAX_SELECTED_PATHS = 64 };
static uint64_t selected[MAX_SELECTED_PATHS];
static size_t selected_count;
static FILE* paths;
static bool failed;
static bool prepared;
static uint64_t captured_count;

static bool is_selected(uint64_t id) {
    for (size_t i = 0; i < selected_count; ++i) {
        if (selected[i] == id) return true;
    }
    return false;
}

static void add_vec(json_object* o, const char* key, Vec3 v) {
    json_object* a = json_object_new_array();
    json_object_array_add(a, json_object_new_double(v.x));
    json_object_array_add(a, json_object_new_double(v.y));
    json_object_array_add(a, json_object_new_double(v.z));
    json_object_object_add(o, key, a);
}
static void add_u(json_object* o, const char* key, uint64_t v) {
    json_object_object_add(o, key, json_object_new_uint64(v));
}
static void add_i(json_object* o, const char* key, int v) {
    json_object_object_add(o, key, json_object_new_int(v));
}
static void add_d(json_object* o, const char* key, double v) {
    json_object_object_add(o, key, json_object_new_double(v));
}
static void add_b(json_object* o, const char* key, bool v) {
    json_object_object_add(o, key, json_object_new_boolean(v));
}
static void add_s(json_object* o, const char* key, const char* v) {
    json_object_object_add(o, key, json_object_new_string(v));
}
static bool write_json(FILE* f, json_object* o) {
    bool ok = fprintf(f, "%s\n", json_object_to_json_string_ext(
        o, JSON_C_TO_STRING_PLAIN)) >= 0 && !ferror(f);
    json_object_put(o);
    return ok;
}

static void observe_path(const RuntimeCausticPhotonSample3D* sample,
                         const RuntimeCausticPhotonSceneTrace3D* trace,
                         void* user_data) {
    (void)user_data;
    ray_tracing_headless_photon_termination_observe(sample, trace);
    if (!is_selected(sample->photonId) || failed) return;
    json_object* o = json_object_new_object();
    json_object* events = json_object_new_array();
    add_s(o, "schema", "ray_tracing_photon_transport_path_v1");
    add_u(o, "photon_id", sample->photonId);
    add_u(o, "rng_seed", sample->rngSeed);
    add_i(o, "light_index", sample->lightIndex);
    add_vec(o, "emission_position", sample->position);
    add_vec(o, "emission_direction", sample->direction);
    add_vec(o, "emission_flux", sample->flux);
    add_d(o, "emission_pdf", sample->emissionPdf);
    add_b(o, "succeeded", trace->readback.succeeded);
    add_u(o, "reject_reason", trace->trace.finalState.rejectReason);
    add_s(o, "termination", RuntimeCausticPhotonSceneTermination3D_Label(
        trace->readback.termination));
    for (uint32_t i = 0; i < trace->readback.hitEventCount; ++i) {
        const RuntimeCausticPhotonSceneHitEvent3D* e = &trace->hitEvents[i];
        json_object* h = json_object_new_object();
        add_u(h, "depth", e->depth);
        add_i(h, "scene_object_index", e->hit.sceneObjectIndex);
        add_i(h, "primitive_index", e->hit.primitiveIndex);
        add_i(h, "triangle_index", e->hit.triangleIndex);
        add_i(h, "material_id", e->material.materialId);
        add_d(h, "material_transparency", e->material.transparency);
        add_d(h, "material_ior", e->material.opticalIor);
        add_b(h, "material_thin_walled", e->material.thinWalled);
        add_d(h, "material_diffuse_weight", e->material.bsdf.diffuseWeight);
        add_d(h, "material_specular_weight", e->material.bsdf.specWeight);
        add_d(h, "material_roughness", e->material.bsdf.roughness);
        add_vec(h, "path_start", e->pathStart);
        add_vec(h, "position", e->hit.position);
        add_vec(h, "normal", e->hit.normal);
        add_vec(h, "outgoing_direction", e->bsdfDirection.outgoingDirection);
        add_vec(h, "throughput_before", e->throughputBeforeAttenuation);
        add_vec(h, "throughput_after_attenuation", e->throughputAfterAttenuation);
        add_vec(h, "throughput_after_bsdf", e->bsdfSelection.throughputAfter);
        add_d(h, "path_pdf_before", e->pathPdfBefore);
        add_d(h, "path_pdf_after", e->pathPdfAfter);
        add_d(h, "branch_pdf", e->bsdfSelection.branchPdf);
        add_s(h, "lobe", RuntimeCausticPhotonBsdfLobe3D_Label(e->bsdfSelection.lobe));
        add_i(h, "segment_medium_object", e->segmentMedium.sceneObjectIndex);
        add_b(h, "segment_medium_air", e->segmentMedium.isAir);
        add_b(h, "medium_transition_attempted", e->mediumTransition.attempted);
        add_b(h, "medium_transition_succeeded", e->mediumTransition.succeeded);
        add_s(h, "medium_transition", RuntimeCausticPhotonMediumTransitionReason3D_Label(
            e->mediumTransition.reason));
        add_u(h, "medium_depth_before", e->mediumTransition.depthBefore);
        add_u(h, "medium_depth_after", e->mediumTransition.depthAfter);
        add_s(h, "termination", RuntimeCausticPhotonSceneTermination3D_Label(e->termination));
        json_object_array_add(events, h);
    }
    json_object_object_add(o, "events", events);
    if (!write_json(paths, o)) failed = true;
    captured_count++;
}

bool ray_tracing_headless_photon_capture_begin(
    const RayTracingAgentRenderRequest* request, const char* ids) {
    char path[PATH_MAX];
    const char* cursor = ids;
    if (!request || !cursor || !cursor[0] || paths ||
        !request->caustic_photon_render_prep_population_enabled ||
        !request->caustic_photon_surface_diagnostics_enabled ||
        !request->caustic_photon_integration_settings.renderContributionEnabled ||
        !request->caustic_photon_integration_settings.surfaceQueryEnabled ||
        request->caustic_photon_integration_settings.productMode != RUNTIME_CAUSTIC_PRODUCT_MODE_PHOTON_MAP ||
        request->caustic_photon_integration_settings.consumer != RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP) return false;
    selected_count = 0;
    while (*cursor) {
        char* end;
        if (*cursor < '0' || *cursor > '9' ||
            selected_count == MAX_SELECTED_PATHS) return false;
        errno = 0;
        uint64_t id = strtoull(cursor, &end, 10);
        if (errno || end == cursor || (*end && *end != ',') || is_selected(id)) return false;
        selected[selected_count++] = id;
        if (!*end) break;
        cursor = end + 1;
        if (!*cursor) return false;
    }
    if (!request->overwrite) {
        const char* stems[] = { "photon_transport_paths", "photon_surface_records",
            "photon_surface_queries", "photon_selected_receiver_queries",
            "photon_surface_query_aggregate", "photon_termination_aggregate",
            "photon_unmatched_paths", "photon_surface_live_queries",
            "photon_receiver_accounting" };
        for (size_t i = 0; i < sizeof(stems) / sizeof(stems[0]); ++i) {
            struct stat info;
            if (snprintf(path, sizeof(path), "%s/%s_%04d.%s",
                         request->output_root, stems[i], request->start_frame,
                         (i == 4 || i == 5 || i == 8) ? "json" : "jsonl") >= (int)sizeof(path)) return false;
            if (stat(path, &info) == 0 || errno != ENOENT) return false;
        }
    }
    if (!config_io_ensure_directory_exists(request->output_root) ||
        snprintf(path, sizeof(path), "%s/photon_transport_paths_%04d.jsonl",
                 request->output_root, request->start_frame) >= (int)sizeof(path)) return false;
    paths = fopen(path, request->overwrite ? "w" : "wx");
    if (!paths) return false;
    if (!ray_tracing_headless_photon_termination_begin(request)) {
        fclose(paths); paths = NULL; return false;
    }
    failed = false;
    prepared = false;
    captured_count = 0;
    RuntimeCausticPhotonPathScheduler3D_SetObserver(observe_path, NULL);
    return true;
}

bool ray_tracing_headless_photon_capture_prepared(
    const RayTracingAgentRenderRequest* request) {
    char path[PATH_MAX];
    if (!paths) return true;
    RuntimeCausticPhotonPathScheduler3D_SetObserver(NULL, NULL);
    prepared = true;
    if (!ray_tracing_headless_photon_termination_end()) failed = true;
    if (!ray_tracing_headless_write_photon_surface_diagnostics(request, request->start_frame)) {
        failed = true;
        return false;
    }
    if (snprintf(path, sizeof(path), "%s/photon_selected_receiver_queries_%04d.jsonl",
                 request->output_root, request->start_frame) >= (int)sizeof(path)) {
        failed = true;
        return false;
    }
    FILE* queries = fopen(path, request->overwrite ? "w" : "wx");
    if (!queries) { failed = true; return false; }
    const uint64_t count = RuntimeNative3DRender_CausticPhotonSurfaceRecordCount();
    for (uint64_t i = 0; i < count; ++i) {
        RuntimeCausticPhotonMapRecord3D r;
        if (!RuntimeNative3DRender_CausticPhotonSurfaceRecordAt(i, &r)) {
            failed = true; break;
        }
        if (!is_selected(r.photonId)) continue;
        HitInfo3D hit = {0};
        RuntimeCausticPhotonMapQueryResult3D q = {0};
        Vec3 radiance;
        hit.position = r.position;
        hit.normal = hit.geometricNormal = hit.shadingNormal = r.normal;
        hit.sceneObjectIndex = r.sceneObjectIndex;
        hit.primitiveIndex = r.primitiveIndex;
        hit.triangleIndex = hit.localTriangleIndex = r.triangleIndex;
        bool sampled = RuntimeCausticPhotonDirectConsumer3D_SampleSurface(&hit, &radiance, &q);
        json_object* o = json_object_new_object();
        add_s(o, "schema", "ray_tracing_photon_receiver_probe_v1");
        add_s(o, "domain", "landing_position_and_receiver_identity_not_camera_pixel");
        add_u(o, "photon_id", r.photonId);
        add_u(o, "depth", r.depth);
        add_i(o, "scene_object_index", r.sceneObjectIndex);
        add_i(o, "material_id", r.materialId);
        add_vec(o, "position", r.position);
        add_b(o, "sampled", sampled);
        add_b(o, "query_hit", q.hit);
        add_b(o, "undersampled", q.undersampled);
        add_u(o, "effective_sample_count", q.effectiveSampleCount);
        add_u(o, "candidate_count", q.candidateCount);
        add_u(o, "receiver_reject_count", q.receiverRejectCount);
        add_u(o, "normal_reject_count", q.normalRejectCount);
        add_d(o, "support_radius", q.supportRadius);
        add_vec(o, "physical_flux", q.physicalFlux);
        add_vec(o, "radiance_with_requested_gain", radiance);
        if (!write_json(queries, o)) { failed = true; break; }
    }
    if (fclose(queries) != 0) failed = true;
    if (captured_count != selected_count) failed = true;
    return !failed;
}

bool ray_tracing_headless_photon_capture_end(void) {
    if (!paths) return true;
    RuntimeCausticPhotonPathScheduler3D_SetObserver(NULL, NULL);
    if (!ray_tracing_headless_photon_termination_end()) failed = true;
    if (fclose(paths) != 0) failed = true;
    paths = NULL;
    return prepared && !failed;
}
