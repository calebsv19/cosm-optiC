#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>
#include <stdio.h>

#include "render/runtime_render_trace_cost_ledger_3d.h"

static bool runtime_caustic_transport_emit_to_triangle_target(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    int triangle_index,
    int sample_index,
    Vec3 target,
    double sample_weight,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    RuntimeMaterialPayload3D payload = {0};
    HitInfo3D hit = {0};
    Vec3 to_target = vec3(0.0, 0.0, 0.0);
    Vec3 path_dir = vec3(0.0, 0.0, 0.0);
    Vec3 throughput = vec3(0.0, 0.0, 0.0);
    Vec3 radiance = vec3(0.0, 0.0, 0.0);
    Ray3D ray = {0};
    Ray3D outgoing = {0};
    double target_distance = 0.0;
    double light_energy = 0.0;
    double volume_footprint_radius = 0.0;
    bool inside_specular_object = false;
    bool emitted = false;
    Vec3 first_surface_normal = vec3(0.0, 0.0, 0.0);
    Vec3 first_geometric_normal = vec3(0.0, 0.0, 0.0);
    bool event_is_refraction = false;
    RuntimeCausticTransportDebugPath3D debug_path = {0};
    bool debug_enabled = RuntimeCausticTransportDebug3D_IsEnabled();

    if (!scene || !light || !diagnostics) return false;
    if (triangle_index < 0 || triangle_index >= scene->triangleMesh.triangleCount) return false;
    to_target = vec3_sub(target, light->position);
    target_distance = vec3_length(to_target);
    if (!(target_distance > 1.0e-6)) return false;

    ray = RuntimeRay3D_Make(light->position, to_target);
    diagnostics->evaluatedPathCount += 1u;
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC,
                                                    1);
    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                         &ray,
                                         1.0e-4,
                                         target_distance + 1.0e-3,
                                         &hit)) {
        return false;
    }
    RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&hit);
    if (hit.triangleIndex != triangle_index) {
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload) || !payload.valid) {
        return false;
    }
    first_geometric_normal = runtime_caustic_transport_hit_geometric_normal(scene, &hit);
    first_surface_normal = runtime_caustic_transport_orient_specular_normal(
        first_geometric_normal,
        ray.direction,
        false);
    if (!runtime_caustic_transport_select_direction_with_normal(&payload,
                                                                first_surface_normal,
                                                                ray.direction,
                                                                &path_dir,
                                                                &throughput,
                                                                &event_is_refraction)) {
        return false;
    }

    diagnostics->transparentHitCount += 1u;
    diagnostics->specularEventCount += 1u;
    sample_weight = runtime_caustic_transport_clamp(sample_weight, 0.0, 1.0);
    light_energy = runtime_caustic_transport_light_attenuation(light, target_distance) *
                   sample_weight;
    volume_footprint_radius = runtime_caustic_transport_clamp(
        fmax(light->radius, 0.0) * 0.85 + target_distance * 0.010,
        0.0,
        0.35);
    radiance = vec3(light->color.x * light_energy * throughput.x,
                    light->color.y * light_energy * throughput.y,
                    light->color.z * light_energy * throughput.z);
    if (!(runtime_caustic_transport_luma(radiance) > 1.0e-9)) return false;

    {
        inside_specular_object = vec3_dot(path_dir, first_surface_normal) < 0.0;
        outgoing = RuntimeRay3D_MakeOffset(hit.position, first_surface_normal, path_dir, 1.0e-4);
    }
    if (debug_enabled) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[triangle_index];
        debug_path.pathId = diagnostics->evaluatedPathCount;
        snprintf(debug_path.emissionPolicy,
                 sizeof(debug_path.emissionPolicy),
                 "%s",
                 RuntimeCausticTransportEmissionPolicy3D_Label(
                     RUNTIME_CAUSTIC_TRANSPORT_EMISSION_TRIANGLE_TARGETS));
        debug_path.lightIndex = light_index;
        snprintf(debug_path.lightId,
                 sizeof(debug_path.lightId),
                 "%s",
                 light->id[0] ? light->id : "compat_light");
        snprintf(debug_path.lightKind,
                 sizeof(debug_path.lightKind),
                 "%s",
                 runtime_caustic_transport_light_kind_label(light->kind));
        debug_path.lightPosition = light->position;
        debug_path.lightRadius = light->radius;
        debug_path.lightIntensity = light->intensity;
        debug_path.lightColor = light->color;
        debug_path.targetTriangleIndex = triangle_index;
        debug_path.targetPrimitiveIndex = triangle->primitiveIndex;
        debug_path.targetSceneObjectIndex = triangle->sceneObjectIndex;
        debug_path.targetSampleIndex = sample_index;
        debug_path.targetPosition = target;
        debug_path.targetDistance = target_distance;
        debug_path.firstHitPosition = hit.position;
        debug_path.firstHitGeometricNormal = first_geometric_normal;
        debug_path.firstHitOrientedNormal = first_surface_normal;
        debug_path.materialId = payload.materialId;
        debug_path.transparency = payload.transparency;
        debug_path.opticalIor = payload.opticalIor;
        debug_path.bsdfIor = payload.bsdf.ior;
        debug_path.roughness = payload.bsdf.roughness;
        debug_path.reflectivity = payload.bsdf.reflectivity;
        debug_path.eligible = runtime_caustic_transport_payload_is_eligible(&payload);
        snprintf(debug_path.eventType,
                 sizeof(debug_path.eventType),
                 "%s",
                 event_is_refraction ? "refraction" : "reflection");
        debug_path.outgoingDirection = path_dir;
        debug_path.throughput = throughput;
        debug_path.initialRadiance = radiance;
        debug_path.insideSpecularObjectAfterEvent = inside_specular_object;
        debug_path.mediumExitSceneObjectIndex = -1;
        debug_path.surfaceReceiverTriangleIndex = -1;
        debug_path.surfaceReceiverPrimitiveIndex = -1;
        debug_path.surfaceReceiverSceneObjectIndex = -1;
    }
    if (cache) {
        Ray3D volume_ray = outgoing;
        Vec3 volume_radiance = radiance;
        if (runtime_caustic_transport_continue_to_outside_medium(
                scene,
                &volume_ray,
                &volume_radiance,
                inside_specular_object,
                hit.sceneObjectIndex,
                max_path_depth,
                diagnostics,
                debug_enabled ? &debug_path : NULL)) {
            emitted = runtime_caustic_transport_deposit_segment(scene,
                                                                cache,
                                                                &volume_ray,
                                                                volume_radiance,
                                                                volume_footprint_radius,
                                                                diagnostics,
                                                                debug_enabled ? &debug_path : NULL) ||
                      emitted;
        }
    }
    if (surface_cache &&
        runtime_caustic_transport_deposit_surface(scene,
                                                  surface_cache,
                                                  &outgoing,
                                                  radiance,
                                                  inside_specular_object,
                                                  hit.sceneObjectIndex,
                                                  max_path_depth,
                                                  surface_footprint_scale,
                                                  surface_radiance_scale,
                                                  receiver_context,
                                                  diagnostics,
                                                  debug_enabled ? &debug_path : NULL)) {
        emitted = true;
    }
    if (emitted) {
        diagnostics->emittedPathCount += 1u;
        if (debug_enabled) {
            RuntimeCausticTransportDebug3D_RecordPath(&debug_path);
        }
        return true;
    }
    return false;
}

void runtime_caustic_transport_emit_to_triangle(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int light_index,
    int triangle_index,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    const double sample_weight = 1.0 / (double)RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT;
    if (!diagnostics) return;
    for (int sample_i = 0;
         sample_i < RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT &&
         (int)diagnostics->evaluatedPathCount < path_budget;
         ++sample_i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[triangle_index];
        Vec3 target = runtime_caustic_transport_triangle_sample_point(triangle, sample_i);
        (void)runtime_caustic_transport_emit_to_triangle_target(scene,
                                                                light,
                                                                light_index,
                                                                triangle_index,
                                                                sample_i,
                                                                target,
                                                                sample_weight,
                                                                cache,
                                                                surface_cache,
                                                                max_path_depth,
                                                                surface_footprint_scale,
                                                                surface_radiance_scale,
                                                                receiver_context,
                                                                diagnostics);
    }
}
