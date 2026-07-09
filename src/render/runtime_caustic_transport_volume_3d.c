#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>

#include "render/runtime_render_trace_cost_ledger_3d.h"
#include "render/runtime_volume_3d_sampling.h"

bool runtime_caustic_transport_deposit_segment(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    const Ray3D* ray,
    Vec3 radiance,
    double base_footprint_radius,
    RuntimeCausticTransport3DDiagnostics* diagnostics,
    RuntimeCausticTransportDebugPath3D* debug_path) {
    double t_enter = 0.0;
    double t_exit = 0.0;
    double step = 0.0;
    double t = 0.0;
    bool deposited = false;

    if (!scene || !cache || !ray || !diagnostics) return false;
    if (!RuntimeVolume3D_ClipRayToBounds(&scene->volume,
                                         ray,
                                         1.0e-4,
                                         1.0e6,
                                         &t_enter,
                                         &t_exit)) {
        if (debug_path) {
            debug_path->volumeClipHit = false;
        }
        return false;
    }
    if (debug_path) {
        debug_path->volumeClipHit = true;
        debug_path->volumeTEnter = t_enter;
        debug_path->volumeTExit = t_exit;
    }
    step = fmax(scene->volume.grid.voxelSize * 0.75, 1.0e-4);
    diagnostics->volumeSegmentCount += 1u;
    for (t = t_enter + step * 0.5; t <= t_exit; t += step) {
        Vec3 p = vec3_add(ray->origin, vec3_scale(ray->direction, t));
        double attenuation = 1.0 / (1.0 + 0.10 * fmax(t - t_enter, 0.0));
        double perpendicular_radius = runtime_caustic_transport_clamp(
            base_footprint_radius + (fmax(t - t_enter, 0.0) * 0.060) + (step * 0.75),
            scene->volume.grid.voxelSize * 2.50,
            scene->volume.grid.voxelSize * 5.00);
        double axial_radius = runtime_caustic_transport_clamp(
            perpendicular_radius + (step * 1.35) + (fmax(t - t_enter, 0.0) * 0.11),
            scene->volume.grid.voxelSize * 3.00,
            scene->volume.grid.voxelSize * 6.50);
        double effective_radius = ((2.0 * perpendicular_radius) + axial_radius) / 3.0;
        Vec3 deposit = vec3_scale(radiance, attenuation * step * 0.020);
        if (debug_path) {
            if (debug_path->volumeStepCount == 0) {
                debug_path->volumeFirstDepositPosition = p;
                debug_path->footprintRadiusMin = effective_radius;
                debug_path->footprintRadiusMax = effective_radius;
            } else {
                if (effective_radius < debug_path->footprintRadiusMin) {
                    debug_path->footprintRadiusMin = effective_radius;
                }
                if (effective_radius > debug_path->footprintRadiusMax) {
                    debug_path->footprintRadiusMax = effective_radius;
                }
            }
            debug_path->volumeLastDepositPosition = p;
            debug_path->volumeStepCount += 1;
        }
        diagnostics->depositAttemptCount += 1u;
        if (RuntimeCausticVolumeCache3D_DepositDirectionalFootprintAtPosition(
                cache,
                p,
                ray->direction,
                perpendicular_radius,
                axial_radius,
                deposit.x,
                deposit.y,
                deposit.z)) {
            double luma = runtime_caustic_transport_luma(deposit);
            diagnostics->depositAcceptedCount += 1u;
            diagnostics->totalRadianceR += deposit.x;
            diagnostics->totalRadianceG += deposit.y;
            diagnostics->totalRadianceB += deposit.z;
            if (luma > diagnostics->maxRadiance) diagnostics->maxRadiance = luma;
            if (debug_path) {
                debug_path->volumeDepositAcceptedCount += 1u;
                debug_path->volumeDepositedRadiance =
                    vec3_add(debug_path->volumeDepositedRadiance, deposit);
            }
            deposited = true;
        } else {
            diagnostics->depositRejectedCount += 1u;
            if (debug_path) {
                debug_path->volumeDepositRejectedCount += 1u;
            }
        }
    }
    return deposited;
}

bool runtime_caustic_transport_continue_to_outside_medium(
    const RuntimeScene3D* scene,
    Ray3D* io_ray,
    Vec3* io_radiance,
    bool inside_specular_object,
    int current_specular_object_index,
    int max_path_depth,
    RuntimeCausticTransport3DDiagnostics* diagnostics,
    RuntimeCausticTransportDebugPath3D* debug_path) {
    int remaining_specular_depth = 0;

    if (!scene || !io_ray || !io_radiance || !diagnostics) return false;
    if (!inside_specular_object) return true;

    remaining_specular_depth = max_path_depth > 1 ? max_path_depth - 1 : 0;
    while (inside_specular_object && remaining_specular_depth > 0) {
        HitInfo3D exit_hit = {0};
        RuntimeMaterialPayload3D exit_payload = {0};
        Vec3 geometric_normal = vec3(0.0, 0.0, 0.0);
        Vec3 surface_normal = vec3(0.0, 0.0, 0.0);
        Vec3 next_direction = vec3(0.0, 0.0, 0.0);
        Vec3 next_throughput = vec3(0.0, 0.0, 0.0);
        bool is_refraction = false;

        RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC,
                                                        1);
        if (!RuntimeRay3D_TraceSceneFirstHit(scene, io_ray, 1.0e-4, 1.0e6, &exit_hit)) {
            return true;
        }
        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&exit_hit);
        if (exit_hit.sceneObjectIndex != current_specular_object_index) {
            return true;
        }
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&exit_hit, &exit_payload) ||
            !runtime_caustic_transport_payload_is_eligible(&exit_payload)) {
            return false;
        }

        geometric_normal = runtime_caustic_transport_hit_geometric_normal(scene, &exit_hit);
        surface_normal = runtime_caustic_transport_orient_specular_normal(
            geometric_normal,
            io_ray->direction,
            inside_specular_object);
        if (!runtime_caustic_transport_select_direction_with_normal(&exit_payload,
                                                                    surface_normal,
                                                                    io_ray->direction,
                                                                    &next_direction,
                                                                    &next_throughput,
                                                                    &is_refraction)) {
            return false;
        }

        diagnostics->transparentHitCount += 1u;
        diagnostics->specularEventCount += 1u;
        io_radiance->x *= next_throughput.x;
        io_radiance->y *= next_throughput.y;
        io_radiance->z *= next_throughput.z;
        if (!(runtime_caustic_transport_luma(*io_radiance) > 1.0e-9)) {
            return false;
        }

        inside_specular_object = vec3_dot(next_direction, surface_normal) < 0.0;
        current_specular_object_index = exit_hit.sceneObjectIndex;
        *io_ray = RuntimeRay3D_MakeOffset(exit_hit.position,
                                          surface_normal,
                                          next_direction,
                                          1.0e-4);
        remaining_specular_depth -= 1;

        if (debug_path) {
            debug_path->continuationEventCount += 1u;
            debug_path->mediumExitSceneObjectIndex = exit_hit.sceneObjectIndex;
            debug_path->mediumExitPosition = exit_hit.position;
            debug_path->mediumExitDirection = next_direction;
            debug_path->exitedSpecularObjectBeforeVolumeDeposit =
                !inside_specular_object && is_refraction;
        }
        if (!inside_specular_object) {
            return true;
        }
    }

    return !inside_specular_object;
}
