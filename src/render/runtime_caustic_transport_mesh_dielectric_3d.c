#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>

typedef enum {
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_INVALID_PROFILE = 0,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_SAMPLE,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_MISS,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_WRONG_OBJECT,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_REFRACTION,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_MISS,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_WRONG_OBJECT,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_REFRACTION,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_INSIDE_DISTANCE,
    RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_THROUGHPUT
} RuntimeCausticMeshDielectricRejectReason3D;

static void runtime_caustic_transport_mesh_dielectric_note_reject(
    RuntimeCausticTransport3DDiagnostics* diagnostics,
    RuntimeCausticMeshDielectricRejectReason3D reason) {
    if (!diagnostics) return;
    switch (reason) {
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_INVALID_PROFILE:
            diagnostics->meshDielectricLensRejectInvalidProfileCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_SAMPLE:
            diagnostics->meshDielectricLensRejectSampleCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_MISS:
            diagnostics->meshDielectricLensRejectEntryMissCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_WRONG_OBJECT:
            diagnostics->meshDielectricLensRejectEntryWrongObjectCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_REFRACTION:
            diagnostics->meshDielectricLensRejectEntryRefractionCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_MISS:
            diagnostics->meshDielectricLensRejectExitMissCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_WRONG_OBJECT:
            diagnostics->meshDielectricLensRejectExitWrongObjectCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_REFRACTION:
            diagnostics->meshDielectricLensRejectExitRefractionCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_INSIDE_DISTANCE:
            diagnostics->meshDielectricLensRejectInsideDistanceCount += 1u;
            break;
        case RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_THROUGHPUT:
            diagnostics->meshDielectricLensRejectThroughputCount += 1u;
            break;
    }
}

static void runtime_caustic_transport_mesh_dielectric_note_accept(
    RuntimeCausticTransport3DDiagnostics* diagnostics,
    double inside_distance,
    double entry_cosine,
    double exit_cosine) {
    if (!diagnostics) return;
    if (diagnostics->meshDielectricLensTraversalAcceptedCount == 0u) {
        diagnostics->meshDielectricLensInsideDistanceMin = inside_distance;
        diagnostics->meshDielectricLensInsideDistanceMax = inside_distance;
        diagnostics->meshDielectricLensEntryCosineMin = entry_cosine;
        diagnostics->meshDielectricLensEntryCosineMax = entry_cosine;
        diagnostics->meshDielectricLensExitCosineMin = exit_cosine;
        diagnostics->meshDielectricLensExitCosineMax = exit_cosine;
    } else {
        diagnostics->meshDielectricLensInsideDistanceMin =
            fmin(diagnostics->meshDielectricLensInsideDistanceMin, inside_distance);
        diagnostics->meshDielectricLensInsideDistanceMax =
            fmax(diagnostics->meshDielectricLensInsideDistanceMax, inside_distance);
        diagnostics->meshDielectricLensEntryCosineMin =
            fmin(diagnostics->meshDielectricLensEntryCosineMin, entry_cosine);
        diagnostics->meshDielectricLensEntryCosineMax =
            fmax(diagnostics->meshDielectricLensEntryCosineMax, entry_cosine);
        diagnostics->meshDielectricLensExitCosineMin =
            fmin(diagnostics->meshDielectricLensExitCosineMin, exit_cosine);
        diagnostics->meshDielectricLensExitCosineMax =
            fmax(diagnostics->meshDielectricLensExitCosineMax, exit_cosine);
    }
    diagnostics->meshDielectricLensTraversalAcceptedCount += 1u;
    diagnostics->meshDielectricLensInsideDistanceSum += inside_distance;
    diagnostics->meshDielectricLensEntryCosineSum += entry_cosine;
    diagnostics->meshDielectricLensExitCosineSum += exit_cosine;
}

static void runtime_caustic_transport_mesh_dielectric_build_basis(Vec3 normal,
                                                                  Vec3* out_u,
                                                                  Vec3* out_v) {
    Vec3 n = vec3_normalize(normal);
    Vec3 helper = fabs(n.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 u = vec3_normalize(vec3_cross(helper, n));
    Vec3 v = vec3_normalize(vec3_cross(n, u));
    if (!(vec3_length(u) > 1.0e-9) || !(vec3_length(v) > 1.0e-9)) {
        u = vec3(1.0, 0.0, 0.0);
        v = vec3(0.0, 0.0, 1.0);
    }
    if (out_u) *out_u = u;
    if (out_v) *out_v = v;
}

static bool runtime_caustic_transport_mesh_dielectric_entry_sample(
    const RuntimeCausticTransportMeshDielectric3D* mesh_dielectric,
    const RuntimeCausticLensTraversalProfile3D* profile,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    Vec3* out_ray_origin,
    Vec3* out_target,
    Vec3* out_ray_dir) {
    const RuntimeTriangle3D* triangle = NULL;
    Vec3 edge_u = vec3(0.0, 0.0, 0.0);
    Vec3 edge_v = vec3(0.0, 0.0, 0.0);
    Vec3 normal = vec3(0.0, 1.0, 0.0);
    Vec3 basis_u = vec3(1.0, 0.0, 0.0);
    Vec3 basis_v = vec3(0.0, 0.0, 1.0);
    Vec3 centroid = vec3(0.0, 0.0, 0.0);
    Vec3 ray_origin = vec3(0.0, 0.0, 0.0);
    Vec3 target = vec3(0.0, 0.0, 0.0);
    Vec3 ray_dir = vec3(0.0, 0.0, 0.0);
    double aperture_radius = 0.25;
    double aperture_scale = 1.0;

    if (!mesh_dielectric || !profile || !light || !sample ||
        !out_ray_origin || !out_target || !out_ray_dir) {
        return false;
    }
    triangle = &mesh_dielectric->entryTriangle;
    edge_u = vec3_sub(triangle->p1, triangle->p0);
    edge_v = vec3_sub(triangle->p2, triangle->p0);
    normal = vec3_normalize(triangle->normal);
    if (!(vec3_length(normal) > 1.0e-9)) {
        normal = vec3_normalize(vec3_cross(edge_u, edge_v));
    }
    if (!(vec3_length(edge_u) > 1.0e-9) ||
        !(vec3_length(edge_v) > 1.0e-9) ||
        !(vec3_length(normal) > 1.0e-9)) {
        return false;
    }
    basis_u = vec3_normalize(edge_u);
    basis_v = vec3_normalize(vec3_cross(normal, basis_u));
    if (!(vec3_length(basis_v) > 1.0e-9)) {
        runtime_caustic_transport_mesh_dielectric_build_basis(normal, &basis_u, &basis_v);
    }
    centroid = vec3_scale(vec3_add(vec3_add(triangle->p0, triangle->p1),
                                   triangle->p2),
                          1.0 / 3.0);
    aperture_radius = mesh_dielectric->shape.radius > 1.0e-9
                          ? mesh_dielectric->shape.radius
                          : 0.25;
    aperture_scale = profile->apertureRadiusScale;
    target = vec3_add(
        centroid,
        vec3_add(vec3_scale(basis_u,
                            runtime_caustic_transport_clamp(sample->lensU, -0.95, 0.95) *
                                aperture_radius * 0.35 * aperture_scale),
                 vec3_scale(basis_v,
                            runtime_caustic_transport_clamp(sample->lensV, -0.95, 0.95) *
                                aperture_radius * 0.35 * aperture_scale)));
    ray_origin = vec3_add(
        light->position,
        vec3_add(vec3_scale(basis_u,
                            runtime_caustic_transport_clamp(sample->apertureU, -1.0, 1.0) *
                                fmax(light->radius, 0.0) * aperture_scale),
                 vec3_scale(basis_v,
                            runtime_caustic_transport_clamp(sample->apertureV, -1.0, 1.0) *
                                fmax(light->radius, 0.0) * aperture_scale)));
    ray_dir = vec3_normalize(vec3_sub(target, ray_origin));
    if (!(vec3_length(ray_dir) > 1.0e-9)) return false;
    *out_ray_origin = ray_origin;
    *out_target = target;
    *out_ray_dir = ray_dir;
    return true;
}

static bool runtime_caustic_transport_trace_same_object_exit(
    const RuntimeScene3D* scene,
    const RuntimeCausticTransportMeshDielectric3D* mesh_dielectric,
    Vec3 entry_position,
    Vec3 entry_normal,
    Vec3 inside_dir,
    HitInfo3D* out_exit_hit,
    RuntimeCausticMeshDielectricRejectReason3D* out_reject_reason) {
    Ray3D inside_ray;
    HitInfo3D exit_hit = {0};
    if (out_reject_reason) {
        *out_reject_reason = RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_MISS;
    }
    if (!scene || !mesh_dielectric || !out_exit_hit) return false;
    inside_ray = RuntimeRay3D_MakeOffset(entry_position,
                                         entry_normal,
                                         inside_dir,
                                         1.0e-4);
    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                         &inside_ray,
                                         1.0e-5,
                                         1.0e6,
                                         &exit_hit)) {
        if (out_reject_reason) {
            *out_reject_reason = RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_MISS;
        }
        return false;
    }
    if (exit_hit.sceneObjectIndex != mesh_dielectric->sceneObjectIndex) {
        if (out_reject_reason) {
            *out_reject_reason =
                RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_WRONG_OBJECT;
        }
        return false;
    }
    *out_exit_hit = exit_hit;
    return true;
}

static bool runtime_caustic_transport_solve_mesh_dielectric_closed_path(
    const RuntimeScene3D* scene,
    const RuntimeCausticTransportMeshDielectric3D* mesh_dielectric,
    const RuntimeCausticLensLightSample3D* light,
    const RuntimeCausticLensSample3D* sample,
    RuntimeCausticLensPath3D* out_path,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    RuntimeCausticLensPath3D path;
    RuntimeCausticLensInterfaceEvent3D entry_event;
    RuntimeCausticLensInterfaceEvent3D exit_event;
    RuntimeCausticLensTraversalProfile3D traversal_profile;
    Vec3 ray_origin = vec3(0.0, 0.0, 0.0);
    Vec3 target = vec3(0.0, 0.0, 0.0);
    Vec3 ray_dir = vec3(0.0, 0.0, 0.0);
    Vec3 entry_normal = vec3(0.0, 0.0, 0.0);
    Vec3 inside_dir = vec3(0.0, 0.0, 0.0);
    Vec3 exit_normal = vec3(0.0, 0.0, 0.0);
    Vec3 exit_dir = vec3(0.0, 0.0, 0.0);
    Vec3 throughput = vec3(1.0, 1.0, 1.0);
    Ray3D entry_ray;
    HitInfo3D entry_hit = {0};
    HitInfo3D exit_hit = {0};
    RuntimeCausticMeshDielectricRejectReason3D exit_reject_reason =
        RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_MISS;
    double entry_fresnel = 1.0;
    double exit_fresnel = 1.0;
    double inside_distance = 0.0;
    double entry_cosine = 0.0;
    double exit_cosine = 0.0;
    double sample_weight = 1.0;
    double receiver_distance = 1.0;
    bool tir = false;

    if (out_path) RuntimeCausticLensTransport3D_DefaultPath(out_path);
    if (!scene || !mesh_dielectric || !mesh_dielectric->valid ||
        !light || !sample || !out_path) {
        return false;
    }

    RuntimeCausticLensTransport3D_ResolveTraversalProfileFromPayload(
        &mesh_dielectric->payload,
        &traversal_profile);
    if (mesh_dielectric->shape.hasTraversalProfileOverride) {
        traversal_profile = mesh_dielectric->shape.traversalProfileOverride;
        traversal_profile.kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
        RuntimeCausticLensTransport3D_NormalizeTraversalProfile(&traversal_profile);
    }
    if (!(traversal_profile.outsideIor > 1.0e-6) ||
        !(traversal_profile.materialIor > 1.0e-6)) {
        runtime_caustic_transport_mesh_dielectric_note_reject(
            diagnostics,
            RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_INVALID_PROFILE);
        return false;
    }
    if (!runtime_caustic_transport_mesh_dielectric_entry_sample(mesh_dielectric,
                                                                &traversal_profile,
                                                                light,
                                                                sample,
                                                                &ray_origin,
                                                                &target,
                                                                &ray_dir)) {
        runtime_caustic_transport_mesh_dielectric_note_reject(
            diagnostics,
            RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_SAMPLE);
        return false;
    }

    entry_ray = RuntimeRay3D_Make(ray_origin, ray_dir);
    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                         &entry_ray,
                                         1.0e-5,
                                         1.0e6,
                                         &entry_hit)) {
        runtime_caustic_transport_mesh_dielectric_note_reject(
            diagnostics,
            RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_MISS);
        return false;
    }
    if (entry_hit.sceneObjectIndex != mesh_dielectric->sceneObjectIndex) {
        runtime_caustic_transport_mesh_dielectric_note_reject(
            diagnostics,
            RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_WRONG_OBJECT);
        return false;
    }
    entry_normal = vec3_normalize(entry_hit.normal);
    if (!(vec3_length(entry_normal) > 1.0e-9)) {
        entry_normal = vec3_normalize(vec3_scale(ray_dir, -1.0));
    }
    entry_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(ray_dir,
                                                     entry_normal,
                                                     traversal_profile.outsideIor,
                                                     traversal_profile.materialIor);
    if (!RuntimeCausticLensTransport3D_Refract(ray_dir,
                                               entry_normal,
                                               traversal_profile.outsideIor,
                                               traversal_profile.materialIor,
                                               &inside_dir,
                                               &tir)) {
        runtime_caustic_transport_mesh_dielectric_note_reject(
            diagnostics,
            RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_ENTRY_REFRACTION);
        return false;
    }
    if (!runtime_caustic_transport_trace_same_object_exit(scene,
                                                          mesh_dielectric,
                                                          entry_hit.position,
                                                          entry_normal,
                                                          inside_dir,
                                                          &exit_hit,
                                                          &exit_reject_reason)) {
        runtime_caustic_transport_mesh_dielectric_note_reject(diagnostics,
                                                             exit_reject_reason);
        return false;
    }
    exit_normal = vec3_normalize(vec3_scale(exit_hit.normal, -1.0));
    if (!(vec3_length(exit_normal) > 1.0e-9)) {
        exit_normal = vec3_normalize(inside_dir);
    }
    inside_distance = vec3_length(vec3_sub(exit_hit.position, entry_hit.position));
    if (!(inside_distance > 1.0e-6)) {
        runtime_caustic_transport_mesh_dielectric_note_reject(
            diagnostics,
            RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_INSIDE_DISTANCE);
        return false;
    }
    exit_fresnel =
        RuntimeCausticLensTransport3D_FresnelSchlick(inside_dir,
                                                     exit_normal,
                                                     traversal_profile.materialIor,
                                                     traversal_profile.outsideIor);
    if (!RuntimeCausticLensTransport3D_Refract(inside_dir,
                                               exit_normal,
                                               traversal_profile.materialIor,
                                               traversal_profile.outsideIor,
                                               &exit_dir,
                                               &tir)) {
        runtime_caustic_transport_mesh_dielectric_note_reject(
            diagnostics,
            RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_EXIT_REFRACTION);
        return false;
    }
    entry_cosine = runtime_caustic_transport_clamp(-vec3_dot(ray_dir, entry_normal),
                                                   0.0,
                                                   1.0);
    exit_cosine = runtime_caustic_transport_clamp(vec3_dot(inside_dir, exit_normal),
                                                  0.0,
                                                  1.0);

    sample_weight = fmax(sample->sampleWeight, 0.0);
    receiver_distance = sample->receiverDistance > 1.0e-9
                            ? sample->receiverDistance
                            : fmax(mesh_dielectric->shape.radius * 4.0,
                                   mesh_dielectric->shape.height * 4.0);
    throughput = vec3_scale(light->color, fmax(light->intensity, 0.0) * sample_weight);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        entry_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyInterfaceTransmissionProfile(
        throughput,
        exit_fresnel,
        &traversal_profile);
    throughput = RuntimeCausticLensTransport3D_ApplyAbsorptionTintProfile(
        throughput,
        inside_distance,
        &traversal_profile);
    if (!(runtime_caustic_transport_luma(throughput) > 1.0e-9)) {
        runtime_caustic_transport_mesh_dielectric_note_reject(
            diagnostics,
            RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_THROUGHPUT);
        return false;
    }

    RuntimeCausticLensTransport3D_DefaultPath(&path);
    path.valid = true;
    path.shapeKind = RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC;
    path.sceneObjectIndex = mesh_dielectric->sceneObjectIndex;
    path.primitiveIndex = mesh_dielectric->primitiveIndex;
    path.lightSamplePosition = ray_origin;
    path.targetPosition = target;
    path.postExitOrigin = exit_hit.position;
    path.postExitDirection = exit_dir;
    path.throughput = throughput;
    path.sampleWeight = sample_weight;
    path.pathPdf = sample_weight > 1.0e-12 ? 1.0 / sample_weight : 0.0;
    path.insideDistance = inside_distance;
    path.receiverPlaneT = receiver_distance;
    path.receiverCrossing = vec3_add(exit_hit.position,
                                     vec3_scale(exit_dir, receiver_distance));
    path.traversalProfile = traversal_profile;

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&entry_event);
    entry_event.position = entry_hit.position;
    entry_event.normal = entry_normal;
    entry_event.incidentDirection = ray_dir;
    entry_event.outgoingDirection = inside_dir;
    entry_event.etaFrom = traversal_profile.outsideIor;
    entry_event.etaTo = traversal_profile.materialIor;
    entry_event.fresnel = entry_fresnel;
    entry_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &entry_event)) {
        return false;
    }

    RuntimeCausticLensTransport3D_DefaultInterfaceEvent(&exit_event);
    exit_event.position = exit_hit.position;
    exit_event.normal = exit_normal;
    exit_event.incidentDirection = inside_dir;
    exit_event.outgoingDirection = exit_dir;
    exit_event.etaFrom = traversal_profile.materialIor;
    exit_event.etaTo = traversal_profile.outsideIor;
    exit_event.fresnel = exit_fresnel;
    exit_event.distanceInMedium = inside_distance;
    exit_event.refracted = true;
    if (!RuntimeCausticLensTransport3D_AppendInterfaceEvent(&path, &exit_event)) {
        return false;
    }

    *out_path = path;
    runtime_caustic_transport_mesh_dielectric_note_accept(diagnostics,
                                                         inside_distance,
                                                         entry_cosine,
                                                         exit_cosine);
    return true;
}

static bool runtime_caustic_transport_emit_mesh_dielectric_lens_sample(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportMeshDielectric3D* mesh_dielectric,
    const RuntimeCausticLensTraversalProfile3D* traversal_profile_override,
    int sample_index,
    int path_budget,
    int sample_count,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    RuntimeCausticLensLightSample3D lens_light;
    RuntimeCausticLensSample3D sample;
    RuntimeCausticLensPath3D path;
    RuntimeCausticTransportLensPathDepositContext3D lens_context = {0};
    RuntimeCausticTransportMeshDielectric3D active_mesh;
    RuntimeCausticLensShape3D shape;
    double aperture_u = 0.0;
    double aperture_v = 0.0;
    double lens_u = 0.0;
    double lens_v = 0.0;
    double light_distance = 0.0;
    double sample_weight = 0.0;
    double volume_footprint_radius = 0.0;

    if (!scene || !light || !mesh_dielectric || !mesh_dielectric->valid ||
        !diagnostics || sample_count <= 0) {
        return false;
    }

    shape = mesh_dielectric->shape;
    if (traversal_profile_override) {
        shape.hasTraversalProfileOverride = true;
        shape.traversalProfileOverride = *traversal_profile_override;
    }
    active_mesh = *mesh_dielectric;
    active_mesh.shape = shape;
    runtime_caustic_transport_sphere_lens_sample(sample_index,
                                                 sample_count,
                                                 &aperture_u,
                                                 &aperture_v,
                                                 &lens_u,
                                                 &lens_v);

    RuntimeCausticLensTransport3D_DefaultLightSample(&lens_light);
    RuntimeCausticLensTransport3D_DefaultSample(&sample);
    light_distance = vec3_length(vec3_sub(shape.center, light->position));
    lens_light.position = light->position;
    lens_light.radius = fmax(light->radius, shape.radius * 0.025);
    lens_light.intensity = runtime_caustic_transport_light_attenuation(light,
                                                                       light_distance);
    lens_light.color = light->color;
    lens_light.lightIndex = light_index;
    sample_weight = runtime_caustic_transport_analytic_sphere_lens_sample_weight(path_budget,
                                                                                 sample_count);
    sample.apertureU = aperture_u;
    sample.apertureV = aperture_v;
    sample.lensU = lens_u;
    sample.lensV = lens_v;
    sample.sampleWeight = sample_weight;
    sample.receiverDistance = fmax(shape.radius * 4.0, shape.height * 4.0);

    diagnostics->evaluatedPathCount += 1u;
    diagnostics->meshDielectricLensEvaluatedPathCount += 1u;
    diagnostics->meshDielectricLensSampleWeight = sample_weight;
    diagnostics->meshDielectricLensTotalSampleWeight += sample_weight;
    if (!runtime_caustic_transport_solve_mesh_dielectric_closed_path(scene,
                                                                     &active_mesh,
                                                                     &lens_light,
                                                                     &sample,
                                                                     &path,
                                                                     diagnostics) ||
        !path.valid ||
        !(runtime_caustic_transport_luma(path.throughput) > 1.0e-9)) {
        if (path.valid && !(runtime_caustic_transport_luma(path.throughput) > 1.0e-9)) {
            runtime_caustic_transport_mesh_dielectric_note_reject(
                diagnostics,
                RUNTIME_CAUSTIC_MESH_DIELECTRIC_REJECT_THROUGHPUT);
        }
        return false;
    }

    volume_footprint_radius = runtime_caustic_transport_clamp(
        shape.radius * 0.035 + fmax(light->radius, 0.0) * 0.55,
        0.0,
        fmax(shape.radius * 0.35, shape.height));
    lens_context.emissionPolicy =
        RUNTIME_CAUSTIC_TRANSPORT_EMISSION_MESH_DIELECTRIC_LENS;
    lens_context.eventType = "mesh_dielectric_lens";
    lens_context.sceneObjectIndex = mesh_dielectric->sceneObjectIndex;
    lens_context.primitiveIndex = mesh_dielectric->primitiveIndex;
    lens_context.sampleIndex = sample_index;
    lens_context.payload = &mesh_dielectric->payload;
    lens_context.volumeFootprintRadius = volume_footprint_radius;
    lens_context.emittedCounter = &diagnostics->meshDielectricLensEmittedPathCount;
    return runtime_caustic_transport_deposit_lens_path(scene,
                                                       light,
                                                       light_index,
                                                       &path,
                                                       &lens_context,
                                                       cache,
                                                       surface_cache,
                                                       max_path_depth,
                                                       surface_footprint_scale,
                                                       surface_radiance_scale,
                                                       receiver_context,
                                                       diagnostics);
}

void runtime_caustic_transport_emit_mesh_dielectric_lens(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    const RuntimeCausticTransportMeshDielectric3D* mesh_dielectric,
    const RuntimeCausticLensTraversalProfile3D* traversal_profile_override,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    int sample_count = path_budget;
    if (!scene || !light || !mesh_dielectric || !diagnostics) return;
    if (sample_count <= 0) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_SEED_SAMPLE_COUNT;
    }
    if (sample_count > RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT) {
        sample_count = RUNTIME_CAUSTIC_TRANSPORT_SPHERE_LENS_MAX_SAMPLE_COUNT;
    }
    for (int sample_i = 0;
         sample_i < sample_count && (int)diagnostics->evaluatedPathCount < path_budget;
         ++sample_i) {
        (void)runtime_caustic_transport_emit_mesh_dielectric_lens_sample(
            scene,
            light,
            light_index,
            mesh_dielectric,
            traversal_profile_override,
            sample_i,
            path_budget,
            sample_count,
            cache,
            surface_cache,
            max_path_depth,
            surface_footprint_scale,
            surface_radiance_scale,
            receiver_context,
            diagnostics);
    }
}
