#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>
#include <string.h>

#include "material/material.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

void runtime_caustic_transport_prepare_surface_receiver_fallback(
    RuntimeCausticTransportSurfaceReceiverContext3D* context,
    const RuntimeScene3D* scene) {
    double best_z = 1.0e30;
    Vec3 position_sum = vec3(0.0, 0.0, 0.0);
    Vec3 normal_sum = vec3(0.0, 0.0, 0.0);
    int position_count = 0;
    if (!context) return;
    memset(context, 0, sizeof(*context));
    HitInfo3D_Reset(&context->surfaceReceiverFallback);
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
        if (context->receiverCandidateCount <
            RUNTIME_CAUSTIC_TRANSPORT_RECEIVER_CANDIDATE_CAP) {
            context->receiverCandidateIndices[context->receiverCandidateCount++] = tri_i;
        }
        if (!context->hasSurfaceReceiverFallback ||
            centroid.z < best_z - 1.0e-6) {
            best_z = centroid.z;
            context->surfaceReceiverFallback = candidate;
            context->hasSurfaceReceiverFallback = true;
            position_sum = centroid;
            normal_sum = candidate.normal;
            position_count = 1;
        } else if (fabs(centroid.z - best_z) <= 1.0e-6) {
            position_sum = vec3_add(position_sum, centroid);
            normal_sum = vec3_add(normal_sum, candidate.normal);
            position_count += 1;
        }
    }
    if (context->hasSurfaceReceiverFallback && position_count > 0) {
        context->surfaceReceiverFallback.position =
            vec3_scale(position_sum, 1.0 / (double)position_count);
        context->surfaceReceiverFallback.normal =
            vec3_length(normal_sum) > 1.0e-9 ? vec3_normalize(normal_sum)
                                             : context->surfaceReceiverFallback.normal;
        context->surfaceReceiverFallback.sceneObjectIndex = -1;
    }
}

void runtime_caustic_transport_disable_surface_receiver_fallback(
    RuntimeCausticTransportSurfaceReceiverContext3D* context) {
    if (!context) return;
    context->hasSurfaceReceiverFallback = false;
    HitInfo3D_Reset(&context->surfaceReceiverFallback);
}

Vec3 runtime_caustic_transport_hit_geometric_normal(const RuntimeScene3D* scene,
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
    const RuntimeCausticTransportSurfaceReceiverContext3D* context,
    HitInfo3D* out_receiver) {
    double best_d2 = 1.0e30;
    HitInfo3D best = {0};
    bool found = false;
    const double max_projection_distance = 2.50;

    if (!scene || !ray || !context || !out_receiver) return false;
    HitInfo3D_Reset(&best);
    for (int candidate_i = 0;
         candidate_i < context->receiverCandidateCount;
         ++candidate_i) {
        int tri_i = context->receiverCandidateIndices[candidate_i];
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

Vec3 runtime_caustic_transport_orient_specular_normal(Vec3 normal,
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

bool runtime_caustic_transport_select_direction_with_normal(
    const RuntimeMaterialPayload3D* payload,
    Vec3 surface_normal,
    Vec3 incident_dir,
    Vec3* out_direction,
    Vec3* out_throughput,
    bool* out_is_refraction) {
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

    if (out_is_refraction) *out_is_refraction = false;
    if (dielectric.hasRefraction && transmission_weight >= reflection_weight * 0.75) {
        direction = dielectric.refractionDir;
        throughput = vec3(payload->baseColorR * transmission_weight,
                          payload->baseColorG * transmission_weight,
                          payload->baseColorB * transmission_weight);
        if (out_is_refraction) *out_is_refraction = true;
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

bool runtime_caustic_transport_deposit_surface(
    const RuntimeScene3D* scene,
    RuntimeCausticSurfaceCache3D* cache,
    const Ray3D* ray,
    Vec3 radiance,
    bool inside_specular_object,
    int current_specular_object_index,
    int max_path_depth,
    double surface_footprint_scale,
    double surface_radiance_scale,
    const RuntimeCausticTransportSurfaceReceiverContext3D* receiver_context,
    RuntimeCausticTransport3DDiagnostics* diagnostics,
    RuntimeCausticTransportDebugPath3D* debug_path) {
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
    remaining_specular_depth = max_path_depth > 1 ? max_path_depth - 1 : 0;

    for (;;) {
        RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC,
                                                        1);
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
                    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
                        RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC,
                        tangent_skip + 1);
                    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                                         &reverse_ray,
                                                         1.0e-7,
                                                         remaining_tangent_distance,
                                                         &tangent_receiver)) {
                        break;
                    }
                    RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&tangent_receiver);
                    if (!RuntimeMaterialPayload3D_ResolveFromHit(&tangent_receiver,
                                                                 &tangent_payload) ||
                        !runtime_caustic_transport_payload_is_eligible(&tangent_payload)) {
                        receiver = tangent_receiver;
                        diagnostics->surfaceReceiverHitCount += 1u;
                        goto runtime_caustic_transport_surface_receiver_ready;
                    }
                    remaining_tangent_distance -= fmax(tangent_receiver.t, 1.0e-7);
                    reverse_ray = RuntimeRay3D_MakeOffset(tangent_receiver.position,
                                                          HitInfo3D_OffsetNormal(&tangent_receiver),
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
                        RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
                            RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC,
                            receiver_probe_skip + 1);
                        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                                             &receiver_probe,
                                                             1.0e-6,
                                                             remaining_probe_distance,
                                                             &projected_receiver)) {
                            break;
                        }
                        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(
                            &projected_receiver);
                        if (!RuntimeMaterialPayload3D_ResolveFromHit(&projected_receiver,
                                                                     &projected_payload) ||
                            !runtime_caustic_transport_payload_is_eligible(&projected_payload)) {
                            receiver = projected_receiver;
                            diagnostics->surfaceReceiverHitCount += 1u;
                            goto runtime_caustic_transport_surface_receiver_ready;
                        }
                        remaining_probe_distance -= fmax(projected_receiver.t, 1.0e-6);
                        receiver_probe = RuntimeRay3D_MakeOffset(projected_receiver.position,
                                                                 HitInfo3D_OffsetNormal(&projected_receiver),
                                                                 receiver_probe.direction,
                                                                 1.0e-5);
                    }
                }
                if (runtime_caustic_transport_find_projected_surface_receiver(scene,
                                                                              &current_ray,
                                                                              receiver_context,
                                                                              &receiver)) {
                    diagnostics->surfaceReceiverHitCount += 1u;
                    goto runtime_caustic_transport_surface_receiver_ready;
                }
                if (receiver_context &&
                    receiver_context->hasSurfaceReceiverFallback) {
                    receiver = receiver_context->surfaceReceiverFallback;
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
        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&receiver);
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
                                                                        &next_throughput,
                                                                        NULL)) {
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
    radius = runtime_caustic_transport_clamp(receiver.t * 0.180 * surface_footprint_scale,
                                             0.025,
                                             1.25);
    attenuation = 1.0 / (1.0 + receiver.t * receiver.t * 0.10);
    deposit = vec3_scale(radiance, attenuation * 0.070 * surface_radiance_scale);
    if (!RuntimeCausticSurfaceCache3D_DepositAtHit(cache,
                                                   &receiver,
                                                   radius,
                                                   deposit.x,
                                                   deposit.y,
                                                   deposit.z)) {
        return false;
    }
    if (debug_path) {
        Vec3 receiver_normal = vec3_length(receiver.normal) > 1.0e-9
                                   ? vec3_normalize(receiver.normal)
                                   : vec3(0.0, 0.0, 1.0);
        debug_path->surfaceReceiverResolved = true;
        debug_path->surfaceReceiverTriangleIndex = receiver.triangleIndex;
        debug_path->surfaceReceiverPrimitiveIndex = receiver.primitiveIndex;
        debug_path->surfaceReceiverSceneObjectIndex = receiver.sceneObjectIndex;
        debug_path->surfaceReceiverPosition = receiver.position;
        debug_path->surfaceReceiverNormal = receiver_normal;
        debug_path->surfaceReceiverT = receiver.t;
        debug_path->surfaceReceiverFootprintRadius = radius;
        debug_path->surfaceReceiverNormalDotRay =
            vec3_dot(receiver_normal, vec3_normalize(current_ray.direction));
        debug_path->surfaceReceiverDepositedRadiance = deposit;
    }
    luma = runtime_caustic_transport_luma(deposit);
    diagnostics->totalRadianceR += deposit.x;
    diagnostics->totalRadianceG += deposit.y;
    diagnostics->totalRadianceB += deposit.z;
    if (luma > diagnostics->maxRadiance) diagnostics->maxRadiance = luma;
    return true;
}
