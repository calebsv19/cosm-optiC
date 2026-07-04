#include "render/runtime_caustic_transport_3d.h"

#include <math.h>
#include <string.h>

#include "material/material.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_volume_3d_sampling.h"

enum {
    RUNTIME_CAUSTIC_TRANSPORT_DEFAULT_PATH_BUDGET = 256,
    RUNTIME_CAUSTIC_TRANSPORT_MAX_PATH_BUDGET = 4096,
    RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT = 5,
    RUNTIME_CAUSTIC_TRANSPORT_RECEIVER_CANDIDATE_CAP = 512
};

static RuntimeCausticTransport3DRequestState g_caustic_transport_state = {0};
static bool g_caustic_transport_has_surface_receiver_fallback = false;
static HitInfo3D g_caustic_transport_surface_receiver_fallback = {0};
static int g_caustic_transport_receiver_candidate_indices[
    RUNTIME_CAUSTIC_TRANSPORT_RECEIVER_CANDIDATE_CAP] = {0};
static int g_caustic_transport_receiver_candidate_count = 0;

static double runtime_caustic_transport_clamp(double value,
                                              double min_value,
                                              double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_caustic_transport_luma(Vec3 rgb) {
    return 0.2126 * rgb.x + 0.7152 * rgb.y + 0.0722 * rgb.z;
}

static Vec3 runtime_caustic_transport_triangle_sample_point(const RuntimeTriangle3D* triangle,
                                                            int sample_index) {
    static const double barycentric[RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT][3] = {
        {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
        {0.60, 0.20, 0.20},
        {0.20, 0.60, 0.20},
        {0.20, 0.20, 0.60},
        {0.45, 0.45, 0.10}
    };
    const double* b = NULL;
    if (!triangle) return vec3(0.0, 0.0, 0.0);
    if (sample_index < 0 || sample_index >= RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT) {
        sample_index = 0;
    }
    b = barycentric[sample_index];
    return vec3_add(vec3_add(vec3_scale(triangle->p0, b[0]),
                             vec3_scale(triangle->p1, b[1])),
                    vec3_scale(triangle->p2, b[2]));
}

static double runtime_caustic_transport_light_attenuation(
    const RuntimeLightSource3D* light,
    double distance_to_target) {
    double falloff = light ? light->falloffDistance : 1.0;
    double d = fmax(distance_to_target, 1.0e-4);
    if (!light) return 0.0;
    if (!(falloff > 1.0e-6)) falloff = 1.0;
    switch (light->falloffMode) {
        case FORWARD_FALLOFF_MODE_NONE:
            return light->intensity;
        case FORWARD_FALLOFF_MODE_LINEAR:
            return light->intensity / (1.0 + (d / falloff));
        case FORWARD_FALLOFF_MODE_QUADRATIC:
        default: {
            double nd = d / falloff;
            return light->intensity / (1.0 + nd * nd);
        }
    }
}

static bool runtime_caustic_transport_payload_is_eligible(
    const RuntimeMaterialPayload3D* payload) {
    if (!payload || !payload->valid) return false;
    if (payload->materialId == MATERIAL_PRESET_TRANSPARENT) return true;
    if (payload->transparency > 1.0e-6) return true;
    if (payload->opticalIor > 1.0001 || payload->bsdf.ior > 1.0001) return true;
    return payload->bsdf.reflectivity > 0.10 && payload->bsdf.roughness <= 0.35;
}

static void runtime_caustic_transport_prepare_surface_receiver_fallback(
    const RuntimeScene3D* scene) {
    double best_z = 1.0e30;
    Vec3 position_sum = vec3(0.0, 0.0, 0.0);
    Vec3 normal_sum = vec3(0.0, 0.0, 0.0);
    int position_count = 0;
    HitInfo3D_Reset(&g_caustic_transport_surface_receiver_fallback);
    g_caustic_transport_has_surface_receiver_fallback = false;
    g_caustic_transport_receiver_candidate_count = 0;
    if (!scene) return;
    for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[tri_i];
        HitInfo3D candidate = {0};
        RuntimeMaterialPayload3D candidate_payload = {0};
        Vec3 centroid = vec3_scale(vec3_add(vec3_add(triangle->p0, triangle->p1),
                                            triangle->p2),
                                   1.0 / 3.0);
        HitInfo3D_Reset(&candidate);
        candidate.t = 0.0;
        candidate.position = centroid;
        candidate.triangleIndex = tri_i;
        candidate.primitiveIndex = triangle->primitiveIndex;
        candidate.sceneObjectIndex = triangle->sceneObjectIndex;
        candidate.normal = vec3_length(triangle->normal) > 1.0e-9
                               ? vec3_normalize(triangle->normal)
                               : vec3_normalize(vec3_cross(vec3_sub(triangle->p1, triangle->p0),
                                                           vec3_sub(triangle->p2, triangle->p0)));
        if (RuntimeMaterialPayload3D_ResolveFromHit(&candidate, &candidate_payload) &&
            runtime_caustic_transport_payload_is_eligible(&candidate_payload)) {
            continue;
        }
        if (g_caustic_transport_receiver_candidate_count <
            RUNTIME_CAUSTIC_TRANSPORT_RECEIVER_CANDIDATE_CAP) {
            g_caustic_transport_receiver_candidate_indices[
                g_caustic_transport_receiver_candidate_count++] = tri_i;
        }
        if (!g_caustic_transport_has_surface_receiver_fallback ||
            centroid.z < best_z - 1.0e-6) {
            best_z = centroid.z;
            g_caustic_transport_surface_receiver_fallback = candidate;
            g_caustic_transport_has_surface_receiver_fallback = true;
            position_sum = centroid;
            normal_sum = candidate.normal;
            position_count = 1;
        } else if (fabs(centroid.z - best_z) <= 1.0e-6) {
            position_sum = vec3_add(position_sum, centroid);
            normal_sum = vec3_add(normal_sum, candidate.normal);
            position_count += 1;
        }
    }
    if (g_caustic_transport_has_surface_receiver_fallback && position_count > 0) {
        g_caustic_transport_surface_receiver_fallback.position =
            vec3_scale(position_sum, 1.0 / (double)position_count);
        g_caustic_transport_surface_receiver_fallback.normal =
            vec3_length(normal_sum) > 1.0e-9 ? vec3_normalize(normal_sum)
                                             : g_caustic_transport_surface_receiver_fallback.normal;
        g_caustic_transport_surface_receiver_fallback.sceneObjectIndex = -1;
    }
}

static Vec3 runtime_caustic_transport_hit_geometric_normal(const RuntimeScene3D* scene,
                                                           const HitInfo3D* hit) {
    Vec3 normal = hit ? hit->normal : vec3(0.0, 0.0, 0.0);
    if (scene && hit &&
        hit->triangleIndex >= 0 &&
        hit->triangleIndex < scene->triangleMesh.triangleCount) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[hit->triangleIndex];
        Vec3 edge1 = vec3_sub(triangle->p1, triangle->p0);
        Vec3 edge2 = vec3_sub(triangle->p2, triangle->p0);
        Vec3 geometric = vec3_cross(edge1, edge2);
        if (vec3_length(geometric) > 1.0e-9) {
            normal = vec3_normalize(geometric);
        } else if (vec3_length(triangle->normal) > 1.0e-9) {
            normal = vec3_normalize(triangle->normal);
        }
    }
    return vec3_normalize(normal);
}

static Vec3 runtime_caustic_transport_closest_point_on_triangle(Vec3 p,
                                                                Vec3 a,
                                                                Vec3 b,
                                                                Vec3 c) {
    Vec3 ab = vec3_sub(b, a);
    Vec3 ac = vec3_sub(c, a);
    Vec3 ap = vec3_sub(p, a);
    double d1 = vec3_dot(ab, ap);
    double d2 = vec3_dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) return a;

    Vec3 bp = vec3_sub(p, b);
    double d3 = vec3_dot(ab, bp);
    double d4 = vec3_dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) return b;

    double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        double v = d1 / (d1 - d3);
        return vec3_add(a, vec3_scale(ab, v));
    }

    Vec3 cp = vec3_sub(p, c);
    double d5 = vec3_dot(ab, cp);
    double d6 = vec3_dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) return c;

    double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        double w = d2 / (d2 - d6);
        return vec3_add(a, vec3_scale(ac, w));
    }

    double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return vec3_add(b, vec3_scale(vec3_sub(c, b), w));
    }

    {
        double denom = 1.0 / (va + vb + vc);
        double v = vb * denom;
        double w = vc * denom;
        return vec3_add(vec3_add(a, vec3_scale(ab, v)), vec3_scale(ac, w));
    }
}

static bool runtime_caustic_transport_find_projected_surface_receiver(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    HitInfo3D* out_receiver) {
    double best_d2 = 1.0e30;
    HitInfo3D best = {0};
    bool found = false;
    const double max_projection_distance = 2.50;

    if (!scene || !ray || !out_receiver) return false;
    HitInfo3D_Reset(&best);
    for (int candidate_i = 0;
         candidate_i < g_caustic_transport_receiver_candidate_count;
         ++candidate_i) {
        int tri_i = g_caustic_transport_receiver_candidate_indices[candidate_i];
        const RuntimeTriangle3D* triangle = NULL;
        HitInfo3D candidate = {0};
        RuntimeMaterialPayload3D candidate_payload = {0};
        Vec3 normal = vec3(0.0, 0.0, 1.0);
        Vec3 plane_point = vec3(0.0, 0.0, 0.0);
        double denom = vec3_dot(ray->direction, normal);
        double t_plane = 0.0;
        Vec3 ray_projected = vec3(0.0, 0.0, 0.0);
        Vec3 origin_projected = vec3(0.0, 0.0, 0.0);
        Vec3 projected = vec3(0.0, 0.0, 0.0);
        Vec3 closest = vec3(0.0, 0.0, 0.0);
        Vec3 ray_closest = vec3(0.0, 0.0, 0.0);
        Vec3 origin_closest = vec3(0.0, 0.0, 0.0);
        Vec3 delta = vec3(0.0, 0.0, 0.0);
        Vec3 ray_delta = vec3(0.0, 0.0, 0.0);
        Vec3 origin_delta = vec3(0.0, 0.0, 0.0);
        double ray_d2 = 0.0;
        double origin_d2 = 0.0;
        double d2 = 0.0;

        if (tri_i < 0 || tri_i >= scene->triangleMesh.triangleCount) continue;
        triangle = &scene->triangleMesh.triangles[tri_i];
        normal = vec3_length(triangle->normal) > 1.0e-9
                     ? vec3_normalize(triangle->normal)
                     : vec3_normalize(vec3_cross(vec3_sub(triangle->p1, triangle->p0),
                                                 vec3_sub(triangle->p2, triangle->p0)));
        plane_point = triangle->p0;
        denom = vec3_dot(ray->direction, normal);
        HitInfo3D_Reset(&candidate);
        candidate.position = vec3_scale(vec3_add(vec3_add(triangle->p0, triangle->p1),
                                                 triangle->p2),
                                        1.0 / 3.0);
        candidate.normal = normal;
        candidate.triangleIndex = tri_i;
        candidate.primitiveIndex = triangle->primitiveIndex;
        candidate.sceneObjectIndex = triangle->sceneObjectIndex;
        if (RuntimeMaterialPayload3D_ResolveFromHit(&candidate, &candidate_payload) &&
            runtime_caustic_transport_payload_is_eligible(&candidate_payload)) {
            continue;
        }

        if (fabs(denom) > 1.0e-6) {
            t_plane = vec3_dot(vec3_sub(plane_point, ray->origin), normal) / denom;
            if (t_plane > 0.0) {
                ray_projected = vec3_add(ray->origin, vec3_scale(ray->direction, t_plane));
            } else {
                ray_projected = ray->origin;
            }
        } else {
            ray_projected = ray->origin;
        }
        origin_projected = vec3_sub(ray->origin,
                                    vec3_scale(normal,
                                               vec3_dot(vec3_sub(ray->origin,
                                                                 plane_point),
                                                        normal)));
        ray_closest = runtime_caustic_transport_closest_point_on_triangle(ray_projected,
                                                                         triangle->p0,
                                                                         triangle->p1,
                                                                         triangle->p2);
        origin_closest = runtime_caustic_transport_closest_point_on_triangle(origin_projected,
                                                                            triangle->p0,
                                                                            triangle->p1,
                                                                            triangle->p2);
        ray_delta = vec3_sub(ray_projected, ray_closest);
        origin_delta = vec3_sub(origin_projected, origin_closest);
        ray_d2 = vec3_dot(ray_delta, ray_delta);
        origin_d2 = vec3_dot(origin_delta, origin_delta);
        if (origin_d2 <= max_projection_distance * max_projection_distance) {
            projected = origin_projected;
            closest = origin_closest;
            d2 = origin_d2;
        } else if (ray_d2 <= max_projection_distance * max_projection_distance) {
            projected = ray_projected;
            closest = ray_closest;
            d2 = ray_d2;
        } else if (origin_d2 < ray_d2) {
            projected = origin_projected;
            closest = origin_closest;
            d2 = origin_d2;
        } else {
            projected = ray_projected;
            closest = ray_closest;
            d2 = ray_d2;
        }
        delta = vec3_sub(projected, closest);
        d2 = fmax(d2, vec3_dot(delta, delta));
        if (d2 < best_d2) {
            best_d2 = d2;
            best = candidate;
            best.position = closest;
            best.t = fmax(vec3_length(vec3_sub(closest, ray->origin)), 1.0e-4);
            found = true;
        }
    }

    if (!found || best_d2 > max_projection_distance * max_projection_distance) {
        return false;
    }
    best.sceneObjectIndex = -1;
    *out_receiver = best;
    return true;
}

static Vec3 runtime_caustic_transport_orient_specular_normal(Vec3 normal,
                                                             Vec3 incident_dir,
                                                             bool inside_specular_object) {
    Vec3 oriented = vec3_normalize(normal);
    double dot_ni = vec3_dot(vec3_normalize(incident_dir), oriented);
    if (inside_specular_object) {
        if (dot_ni < 0.0) oriented = vec3_scale(oriented, -1.0);
    } else {
        if (dot_ni > 0.0) oriented = vec3_scale(oriented, -1.0);
    }
    return oriented;
}

static bool runtime_caustic_transport_select_direction_with_normal(
    const RuntimeMaterialPayload3D* payload,
    Vec3 surface_normal,
    Vec3 incident_dir,
    Vec3* out_direction,
    Vec3* out_throughput) {
    RuntimeDielectricTransport3D dielectric = {0};
    double transmission_weight = 0.0;
    double reflection_weight = 0.0;
    Vec3 direction = vec3(0.0, 0.0, 0.0);
    Vec3 throughput = vec3(1.0, 1.0, 1.0);

    if (!payload || !out_direction || !out_throughput) return false;
    if (!runtime_caustic_transport_payload_is_eligible(payload)) return false;
    if (!RuntimeDielectricTransport3D_Resolve(payload,
                                              surface_normal,
                                              incident_dir,
                                              &dielectric)) {
        return false;
    }

    transmission_weight = runtime_caustic_transport_clamp(payload->transparency, 0.0, 1.0);
    if (payload->materialId == MATERIAL_PRESET_TRANSPARENT ||
        payload->opticalIor > 1.0001 || payload->bsdf.ior > 1.0001) {
        transmission_weight = fmax(transmission_weight, 1.0 - dielectric.fresnel);
    }
    reflection_weight = runtime_caustic_transport_clamp(payload->bsdf.reflectivity, 0.0, 1.0) *
                        runtime_caustic_transport_clamp(1.0 - payload->bsdf.roughness, 0.0, 1.0);

    if (dielectric.hasRefraction && transmission_weight >= reflection_weight * 0.75) {
        direction = dielectric.refractionDir;
        throughput = vec3(payload->baseColorR * transmission_weight,
                          payload->baseColorG * transmission_weight,
                          payload->baseColorB * transmission_weight);
    } else {
        direction = dielectric.reflectionDir;
        throughput = vec3(payload->baseColorR * fmax(reflection_weight, dielectric.fresnel),
                          payload->baseColorG * fmax(reflection_weight, dielectric.fresnel),
                          payload->baseColorB * fmax(reflection_weight, dielectric.fresnel));
    }

    if (!(vec3_length(direction) > 1.0e-9)) return false;
    if (!(runtime_caustic_transport_luma(throughput) > 1.0e-9)) return false;
    *out_direction = vec3_normalize(direction);
    *out_throughput = throughput;
    return true;
}

static bool runtime_caustic_transport_deposit_segment(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    const Ray3D* ray,
    Vec3 radiance,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
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
        return false;
    }
    step = fmax(scene->volume.grid.voxelSize * 0.75, 1.0e-4);
    diagnostics->volumeSegmentCount += 1u;
    for (t = t_enter + step * 0.5; t <= t_exit; t += step) {
        Vec3 p = vec3_add(ray->origin, vec3_scale(ray->direction, t));
        double attenuation = 1.0 / (1.0 + 0.10 * fmax(t - t_enter, 0.0));
        Vec3 deposit = vec3_scale(radiance, attenuation * step * 0.020);
        diagnostics->depositAttemptCount += 1u;
        if (RuntimeCausticVolumeCache3D_DepositAtPosition(cache,
                                                          p,
                                                          deposit.x,
                                                          deposit.y,
                                                          deposit.z)) {
            double luma = runtime_caustic_transport_luma(deposit);
            diagnostics->depositAcceptedCount += 1u;
            diagnostics->totalRadianceR += deposit.x;
            diagnostics->totalRadianceG += deposit.y;
            diagnostics->totalRadianceB += deposit.z;
            if (luma > diagnostics->maxRadiance) diagnostics->maxRadiance = luma;
            deposited = true;
        } else {
            diagnostics->depositRejectedCount += 1u;
        }
    }
    return deposited;
}

static bool runtime_caustic_transport_deposit_surface(
    const RuntimeScene3D* scene,
    RuntimeCausticSurfaceCache3D* cache,
    const Ray3D* ray,
    Vec3 radiance,
    bool inside_specular_object,
    int current_specular_object_index,
    RuntimeCausticTransport3DDiagnostics* diagnostics) {
    HitInfo3D receiver = {0};
    RuntimeMaterialPayload3D receiver_payload = {0};
    Ray3D current_ray = {0};
    int remaining_specular_depth = 0;
    int escape_skip_count = 0;
    double radius = 0.0;
    double attenuation = 0.0;
    Vec3 deposit = vec3(0.0, 0.0, 0.0);
    double luma = 0.0;

    if (!scene || !cache || !ray || !diagnostics) return false;
    current_ray = *ray;
    remaining_specular_depth = g_caustic_transport_state.maxPathDepth > 1
                                   ? g_caustic_transport_state.maxPathDepth - 1
                                   : 0;

    for (;;) {
        if (!RuntimeRay3D_TraceSceneFirstHit(scene, &current_ray, 1.0e-4, 1.0e6, &receiver)) {
            if (!inside_specular_object) {
                Ray3D reverse_ray =
                    RuntimeRay3D_Make(current_ray.origin,
                                      vec3_scale(current_ray.direction, -1.0));
                double remaining_tangent_distance = 5.0e-2;
                for (int tangent_skip = 0;
                     tangent_skip < 6 && remaining_tangent_distance > 1.0e-7;
                     ++tangent_skip) {
                    HitInfo3D tangent_receiver = {0};
                    RuntimeMaterialPayload3D tangent_payload = {0};
                    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                                         &reverse_ray,
                                                         1.0e-7,
                                                         remaining_tangent_distance,
                                                         &tangent_receiver)) {
                        break;
                    }
                    if (!RuntimeMaterialPayload3D_ResolveFromHit(&tangent_receiver,
                                                                 &tangent_payload) ||
                        !runtime_caustic_transport_payload_is_eligible(&tangent_payload)) {
                        receiver = tangent_receiver;
                        diagnostics->surfaceReceiverHitCount += 1u;
                        goto runtime_caustic_transport_surface_receiver_ready;
                    }
                    remaining_tangent_distance -= fmax(tangent_receiver.t, 1.0e-7);
                    reverse_ray = RuntimeRay3D_MakeOffset(tangent_receiver.position,
                                                          tangent_receiver.normal,
                                                          reverse_ray.direction,
                                                          1.0e-5);
                }
                {
                    Ray3D receiver_probe = RuntimeRay3D_Make(
                        vec3_add(current_ray.origin, vec3(0.0, 0.0, 1.0e-3)),
                        vec3(0.0, 0.0, -1.0));
                    double remaining_probe_distance = 2.0;
                    for (int receiver_probe_skip = 0;
                         receiver_probe_skip < 6 && remaining_probe_distance > 1.0e-6;
                         ++receiver_probe_skip) {
                        HitInfo3D projected_receiver = {0};
                        RuntimeMaterialPayload3D projected_payload = {0};
                        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                                             &receiver_probe,
                                                             1.0e-6,
                                                             remaining_probe_distance,
                                                             &projected_receiver)) {
                            break;
                        }
                        if (!RuntimeMaterialPayload3D_ResolveFromHit(&projected_receiver,
                                                                     &projected_payload) ||
                            !runtime_caustic_transport_payload_is_eligible(&projected_payload)) {
                            receiver = projected_receiver;
                            diagnostics->surfaceReceiverHitCount += 1u;
                            goto runtime_caustic_transport_surface_receiver_ready;
                        }
                        remaining_probe_distance -= fmax(projected_receiver.t, 1.0e-6);
                        receiver_probe = RuntimeRay3D_MakeOffset(projected_receiver.position,
                                                                 projected_receiver.normal,
                                                                 receiver_probe.direction,
                                                                 1.0e-5);
                    }
                }
                if (runtime_caustic_transport_find_projected_surface_receiver(scene,
                                                                              &current_ray,
                                                                              &receiver)) {
                    diagnostics->surfaceReceiverHitCount += 1u;
                    goto runtime_caustic_transport_surface_receiver_ready;
                }
                if (g_caustic_transport_state.surfaceReceiverFallbackEnabled &&
                    g_caustic_transport_has_surface_receiver_fallback) {
                    receiver = g_caustic_transport_surface_receiver_fallback;
                    receiver.t = fmax(vec3_length(vec3_sub(receiver.position, current_ray.origin)),
                                      4.0);
                    diagnostics->surfaceReceiverHitCount += 1u;
                    diagnostics->surfaceReceiverFallbackCount += 1u;
                    goto runtime_caustic_transport_surface_receiver_ready;
                }
            }
            diagnostics->surfaceReceiverTraceMissCount += 1u;
            return false;
        }
        if (RuntimeMaterialPayload3D_ResolveFromHit(&receiver, &receiver_payload) &&
            runtime_caustic_transport_payload_is_eligible(&receiver_payload)) {
            Vec3 geometric_normal = runtime_caustic_transport_hit_geometric_normal(scene,
                                                                                   &receiver);
            Vec3 surface_normal = runtime_caustic_transport_orient_specular_normal(
                geometric_normal,
                current_ray.direction,
                inside_specular_object);
            Vec3 next_direction = vec3(0.0, 0.0, 0.0);
            Vec3 next_throughput = vec3(0.0, 0.0, 0.0);
            if (remaining_specular_depth <= 0) {
                if (!inside_specular_object &&
                    receiver.sceneObjectIndex == current_specular_object_index &&
                    escape_skip_count < 4) {
                    current_ray = RuntimeRay3D_MakeOffset(receiver.position,
                                                          surface_normal,
                                                          current_ray.direction,
                                                          1.0e-3);
                    escape_skip_count += 1;
                    continue;
                }
                diagnostics->surfaceReceiverDepthRejectCount += 1u;
                return false;
            }
            if (!runtime_caustic_transport_select_direction_with_normal(&receiver_payload,
                                                                        surface_normal,
                                                                        current_ray.direction,
                                                                        &next_direction,
                                                                        &next_throughput)) {
                return false;
            }
            diagnostics->transparentHitCount += 1u;
            diagnostics->specularEventCount += 1u;
            radiance.x *= next_throughput.x;
            radiance.y *= next_throughput.y;
            radiance.z *= next_throughput.z;
            if (!(runtime_caustic_transport_luma(radiance) > 1.0e-9)) {
                return false;
            }
            inside_specular_object = vec3_dot(next_direction, surface_normal) < 0.0;
            current_specular_object_index = receiver.sceneObjectIndex;
            escape_skip_count = 0;
            current_ray = RuntimeRay3D_MakeOffset(receiver.position,
                                                  surface_normal,
                                                  next_direction,
                                                  1.0e-4);
            remaining_specular_depth -= 1;
            continue;
        }
        diagnostics->surfaceReceiverHitCount += 1u;
        break;
    }
runtime_caustic_transport_surface_receiver_ready:
    radius = runtime_caustic_transport_clamp(
        receiver.t * 0.180 * g_caustic_transport_state.surfaceFootprintScale,
        0.025,
        1.25);
    attenuation = 1.0 / (1.0 + receiver.t * receiver.t * 0.10);
    deposit = vec3_scale(radiance,
                         attenuation * 0.070 *
                             g_caustic_transport_state.surfaceRadianceScale);
    if (!RuntimeCausticSurfaceCache3D_DepositAtHit(cache,
                                                   &receiver,
                                                   radius,
                                                   deposit.x,
                                                   deposit.y,
                                                   deposit.z)) {
        return false;
    }
    luma = runtime_caustic_transport_luma(deposit);
    diagnostics->totalRadianceR += deposit.x;
    diagnostics->totalRadianceG += deposit.y;
    diagnostics->totalRadianceB += deposit.z;
    if (luma > diagnostics->maxRadiance) diagnostics->maxRadiance = luma;
    return true;
}

static bool runtime_caustic_transport_emit_to_triangle_target(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int triangle_index,
    Vec3 target,
    double sample_weight,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
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
    bool inside_specular_object = false;
    bool emitted = false;
    Vec3 first_surface_normal = vec3(0.0, 0.0, 0.0);

    if (!scene || !light || !diagnostics) return false;
    if (triangle_index < 0 || triangle_index >= scene->triangleMesh.triangleCount) return false;
    to_target = vec3_sub(target, light->position);
    target_distance = vec3_length(to_target);
    if (!(target_distance > 1.0e-6)) return false;

    ray = RuntimeRay3D_Make(light->position, to_target);
    diagnostics->evaluatedPathCount += 1u;
    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                         &ray,
                                         1.0e-4,
                                         target_distance + 1.0e-3,
                                         &hit)) {
        return false;
    }
    if (hit.triangleIndex != triangle_index) {
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload) || !payload.valid) {
        return false;
    }
    first_surface_normal = runtime_caustic_transport_orient_specular_normal(
        runtime_caustic_transport_hit_geometric_normal(scene, &hit),
        ray.direction,
        false);
    if (!runtime_caustic_transport_select_direction_with_normal(&payload,
                                                                first_surface_normal,
                                                                ray.direction,
                                                                &path_dir,
                                                                &throughput)) {
        return false;
    }

    diagnostics->transparentHitCount += 1u;
    diagnostics->specularEventCount += 1u;
    sample_weight = runtime_caustic_transport_clamp(sample_weight, 0.0, 1.0);
    light_energy = runtime_caustic_transport_light_attenuation(light, target_distance) *
                   sample_weight;
    radiance = vec3(light->color.x * light_energy * throughput.x,
                    light->color.y * light_energy * throughput.y,
                    light->color.z * light_energy * throughput.z);
    if (!(runtime_caustic_transport_luma(radiance) > 1.0e-9)) return false;

    {
        inside_specular_object = vec3_dot(path_dir, first_surface_normal) < 0.0;
        outgoing = RuntimeRay3D_MakeOffset(hit.position, first_surface_normal, path_dir, 1.0e-4);
    }
    if (cache) {
        emitted = runtime_caustic_transport_deposit_segment(scene,
                                                            cache,
                                                            &outgoing,
                                                            radiance,
                                                            diagnostics) ||
                  emitted;
    }
    if (surface_cache &&
        runtime_caustic_transport_deposit_surface(scene,
                                                  surface_cache,
                                                  &outgoing,
                                                  radiance,
                                                  inside_specular_object,
                                                  hit.sceneObjectIndex,
                                                  diagnostics)) {
        emitted = true;
    }
    if (emitted) {
        diagnostics->emittedPathCount += 1u;
        return true;
    }
    return false;
}

static void runtime_caustic_transport_emit_to_triangle(
    const RuntimeScene3D* scene,
    const RuntimeLightSource3D* light,
    int triangle_index,
    int path_budget,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
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
                                                                triangle_index,
                                                                target,
                                                                sample_weight,
                                                                cache,
                                                                surface_cache,
                                                                diagnostics);
    }
}

void RuntimeCausticTransport3D_ResetRequestState(void) {
    memset(&g_caustic_transport_state, 0, sizeof(g_caustic_transport_state));
    g_caustic_transport_state.mode = RUNTIME_CAUSTIC_MODE_OFF;
    g_caustic_transport_state.surfaceRadianceScale = 1.0;
    g_caustic_transport_state.surfaceFootprintScale = 1.0;
    g_caustic_transport_state.surfaceReceiverFallbackEnabled = true;
}

void RuntimeCausticTransport3D_SetRequestState(const RuntimeCausticSettings3D* settings) {
    RuntimeCausticSettings3D defaults;
    const RuntimeCausticSettings3D* src = settings;
    if (!src) {
        RuntimeCausticSettings3D_Default(&defaults);
        src = &defaults;
    }
    memset(&g_caustic_transport_state, 0, sizeof(g_caustic_transport_state));
    g_caustic_transport_state.mode = src->mode;
    g_caustic_transport_state.volumeCacheRequested = src->volumeCacheEnabled;
    g_caustic_transport_state.surfaceCacheRequested = src->surfaceCacheEnabled;
    g_caustic_transport_state.sampleBudget = src->sampleBudget;
    g_caustic_transport_state.maxPathDepth = src->maxPathDepth;
    g_caustic_transport_state.surfaceRadianceScale =
        runtime_caustic_transport_clamp(src->surfaceRadianceScale, 0.0, 128.0);
    g_caustic_transport_state.surfaceFootprintScale =
        runtime_caustic_transport_clamp(src->surfaceFootprintScale, 0.1, 16.0);
    g_caustic_transport_state.surfaceReceiverFallbackEnabled =
        src->surfaceReceiverFallbackEnabled;
    g_caustic_transport_state.enabled =
        src->mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
        (src->volumeCacheEnabled || src->surfaceCacheEnabled);
}

RuntimeCausticTransport3DRequestState RuntimeCausticTransport3D_RequestState(void) {
    return g_caustic_transport_state;
}

bool RuntimeCausticTransport3D_PopulateVolumeCache(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticTransport3DDiagnostics* out_diagnostics) {
    return RuntimeCausticTransport3D_PopulateCaches(scene, cache, NULL, out_diagnostics);
}

bool RuntimeCausticTransport3D_PopulateCaches(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    RuntimeCausticTransport3DDiagnostics* out_diagnostics) {
    RuntimeCausticTransport3DDiagnostics diagnostics = {0};
    int enabled_light_count = 0;
    int path_budget = RUNTIME_CAUSTIC_TRANSPORT_DEFAULT_PATH_BUDGET;
    bool volume_cache_active = false;
    RuntimeLightSource3D compat_light;

    if (out_diagnostics) *out_diagnostics = diagnostics;
    diagnostics.requested = g_caustic_transport_state.enabled;
    diagnostics.active = g_caustic_transport_state.enabled;
    if (!g_caustic_transport_state.enabled || !scene) {
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    volume_cache_active = g_caustic_transport_state.volumeCacheRequested &&
                          cache &&
                          RuntimeVolume3D_HasSampleableDensity(&scene->volume);
    diagnostics.volumeCacheSuppressedNoSampleableVolume =
        g_caustic_transport_state.volumeCacheRequested && !volume_cache_active;
    if (g_caustic_transport_state.volumeCacheRequested &&
        !volume_cache_active &&
        !g_caustic_transport_state.surfaceCacheRequested) {
        if (out_diagnostics) *out_diagnostics = diagnostics;
        return false;
    }
    if (g_caustic_transport_state.sampleBudget > 0) {
        path_budget = g_caustic_transport_state.sampleBudget;
    }
    if (path_budget > RUNTIME_CAUSTIC_TRANSPORT_MAX_PATH_BUDGET) {
        path_budget = RUNTIME_CAUSTIC_TRANSPORT_MAX_PATH_BUDGET;
    }

    if (volume_cache_active) {
        if (!RuntimeCausticVolumeCache3D_AllocateFromVolume(cache, &scene->volume)) {
            RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
        diagnostics.cacheAllocated = true;
    }
    if (g_caustic_transport_state.surfaceCacheRequested) {
        uint64_t capacity = 0u;
        if (!surface_cache) {
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
        capacity = (uint64_t)fmax((double)path_budget * 2.0, 64.0);
        if (!RuntimeCausticSurfaceCache3D_Allocate(surface_cache, capacity)) {
            RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache,
                                                             &diagnostics.surfaceCache);
            if (out_diagnostics) *out_diagnostics = diagnostics;
            return false;
        }
        diagnostics.surfaceCacheAllocated = true;
    }
    runtime_caustic_transport_prepare_surface_receiver_fallback(scene);
    if (!g_caustic_transport_state.surfaceReceiverFallbackEnabled) {
        g_caustic_transport_has_surface_receiver_fallback = false;
        HitInfo3D_Reset(&g_caustic_transport_surface_receiver_fallback);
    }

    enabled_light_count = RuntimeLightSet3D_EnabledCount(&scene->lightSet);
    if (enabled_light_count > 0) {
        for (int light_i = 0; light_i < enabled_light_count; ++light_i) {
            const RuntimeLightSource3D* light =
                RuntimeLightSet3D_GetEnabled(&scene->lightSet, light_i);
            if (!light || light->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE) continue;
            diagnostics.lightCount += 1u;
            for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
                if ((int)diagnostics.evaluatedPathCount >= path_budget) break;
                runtime_caustic_transport_emit_to_triangle(scene,
                                                           light,
                                                           tri_i,
                                                           path_budget,
                                                           volume_cache_active ? cache : NULL,
                                                           g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                                                           &diagnostics);
            }
        }
    } else if (scene->hasLight) {
        RuntimeLightSource3D_Init(&compat_light);
        compat_light.kind = scene->light.radius > 0.0 ? RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE
                                                      : RUNTIME_LIGHT_SOURCE_3D_KIND_POINT;
        compat_light.position = scene->light.position;
        compat_light.radius = scene->light.radius;
        compat_light.intensity = scene->light.intensity;
        compat_light.falloffDistance = scene->light.falloffDistance;
        compat_light.falloffMode = scene->light.falloffMode;
        diagnostics.lightCount = 1u;
        for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
            if ((int)diagnostics.evaluatedPathCount >= path_budget) break;
            runtime_caustic_transport_emit_to_triangle(scene,
                                                       &compat_light,
                                                       tri_i,
                                                       path_budget,
                                                       volume_cache_active ? cache : NULL,
                                                       g_caustic_transport_state.surfaceCacheRequested ? surface_cache : NULL,
                                                       &diagnostics);
        }
    }

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(cache, &diagnostics.cache);
    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache, &diagnostics.surfaceCache);
    if (out_diagnostics) *out_diagnostics = diagnostics;
    return diagnostics.emittedPathCount > 0u &&
           (!volume_cache_active ||
            diagnostics.cache.nonZeroCellCount > 0u) &&
           (!g_caustic_transport_state.surfaceCacheRequested ||
            diagnostics.surfaceCache.recordCount > 0u);
}
