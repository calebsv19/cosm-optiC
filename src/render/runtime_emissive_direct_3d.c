#include "render/runtime_emissive_direct_3d.h"

#include <math.h>

#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_visibility_3d.h"

static const double kRuntimeEmissiveDirect3DEpsilon = 1e-4;
static const double kRuntimeEmissiveDirect3DEnergyScale = 1.25;
static const int kRuntimeEmissiveDirect3DMaxCandidateSamples = 1;
static const int kRuntimeEmissiveDirect3DMaxSelectionAttempts = 4;
static const double kRuntimeEmissiveDirect3DMaxPdf = 1.0e6;

static double runtime_emissive_direct_3d_clamp(double value,
                                               double min_value,
                                               double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_emissive_direct_3d_distance_decay(double distance) {
    return 1.0 / (1.0 + distance * distance);
}

static double runtime_emissive_direct_3d_solid_angle_pdf(double selection_pdf,
                                                         double area,
                                                         double distance,
                                                         double emitter_facing) {
    double pdf = 0.0;

    if (!(selection_pdf > 0.0) ||
        !(area > 1e-12) ||
        !(distance > kRuntimeEmissiveDirect3DEpsilon) ||
        !(emitter_facing > 1e-9)) {
        return 0.0;
    }

    pdf = (selection_pdf / area) * distance * distance / emitter_facing;
    if (!isfinite(pdf)) return 0.0;
    return runtime_emissive_direct_3d_clamp(pdf, 0.0, kRuntimeEmissiveDirect3DMaxPdf);
}

static uint32_t runtime_emissive_direct_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_emissive_direct_3d_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling) {
    uint32_t sx = 0U;
    uint32_t sy = 0U;
    uint32_t sz = 0U;
    uint32_t sequence = 0U;

    if (!hit) return 0U;
    if (sampling) {
        sequence = sampling->sampleSequence;
    }

    sx = (uint32_t)(fabs(hit->position.x) * 4096.0);
    sy = (uint32_t)(fabs(hit->position.y) * 4096.0);
    sz = (uint32_t)(fabs(hit->position.z) * 4096.0);
    return runtime_emissive_direct_3d_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^
        ((uint32_t)(hit->triangleIndex + 1) * 83492791U) ^
        runtime_emissive_direct_3d_hash_u32(sequence ^ 0x27d4eb2dU));
}

static double runtime_emissive_direct_3d_triangle_area(const RuntimeTriangle3D* triangle) {
    Vec3 edge_a = vec3(0.0, 0.0, 0.0);
    Vec3 edge_b = vec3(0.0, 0.0, 0.0);
    Vec3 cross = vec3(0.0, 0.0, 0.0);

    if (!triangle) return 0.0;

    edge_a = vec3_sub(triangle->p1, triangle->p0);
    edge_b = vec3_sub(triangle->p2, triangle->p0);
    cross = vec3_cross(edge_a, edge_b);
    return 0.5 * vec3_length(cross);
}

static Vec3 runtime_emissive_direct_3d_sample_triangle_point(
    const RuntimeTriangle3D* triangle,
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t base_seed,
    int triangle_index) {
    double u = 0.0;
    double v = 0.0;

    if (!triangle) return vec3(0.0, 0.0, 0.0);

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         base_seed ^ (uint32_t)(triangle_index * 2654435761U),
                                         1,
                                         0,
                                         (uint32_t)triangle_index,
                                         &u,
                                         &v);
    if ((u + v) > 1.0) {
        u = 1.0 - u;
        v = 1.0 - v;
    }

    return vec3_add(triangle->p0,
                    vec3_add(vec3_scale(vec3_sub(triangle->p1, triangle->p0), u),
                             vec3_scale(vec3_sub(triangle->p2, triangle->p0), v)));
}

static int runtime_emissive_direct_3d_select_candidate_index(
    const RuntimeEmissiveLightSet3D* light_set,
    double target_weight) {
    int lo = 0;
    int hi = 0;

    if (!light_set || !light_set->candidates || light_set->candidateCount <= 0) {
        return -1;
    }

    if (!(target_weight > 0.0)) {
        return 0;
    }
    if (target_weight >= light_set->totalWeight) {
        return light_set->candidateCount - 1;
    }

    hi = light_set->candidateCount - 1;
    while (lo < hi) {
        const int mid = lo + ((hi - lo) / 2);
        if (target_weight <= light_set->candidates[mid].cumulativeWeight) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

static const RuntimeEmissiveLightCandidate3D*
runtime_emissive_direct_3d_select_candidate(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t base_seed,
    int sample_index,
    int* out_candidate_index) {
    const RuntimeEmissiveLightSet3D* light_set = NULL;
    double u = 0.5;
    double v = 0.5;

    if (out_candidate_index) *out_candidate_index = -1;
    if (!scene || !hit) return NULL;
    light_set = &scene->emissiveLightSet;
    if (!light_set->valid ||
        !light_set->candidates ||
        light_set->candidateCount <= 0 ||
        !(light_set->totalWeight > 0.0)) {
        return NULL;
    }

    for (int attempt = 0; attempt < kRuntimeEmissiveDirect3DMaxSelectionAttempts; ++attempt) {
        const uint32_t dimension = (uint32_t)(173U + (uint32_t)attempt);
        const RuntimeEmissiveLightCandidate3D* candidate = NULL;
        int candidate_index = -1;

        RuntimeNative3DSampling_Stratified2D(sampling,
                                             base_seed ^
                                                 (uint32_t)(sample_index * 0x9e3779b9U) ^
                                                 (uint32_t)(attempt * 0x85ebca6bU),
                                             kRuntimeEmissiveDirect3DMaxCandidateSamples,
                                             sample_index,
                                             dimension,
                                             &u,
                                             &v);
        (void)v;
        candidate_index =
            runtime_emissive_direct_3d_select_candidate_index(light_set,
                                                              u * light_set->totalWeight);
        if (candidate_index < 0 || candidate_index >= light_set->candidateCount) {
            continue;
        }
        candidate = &light_set->candidates[candidate_index];
        if (candidate->sceneObjectIndex < 0 ||
            candidate->sceneObjectIndex == hit->sceneObjectIndex) {
            continue;
        }
        if (out_candidate_index) *out_candidate_index = candidate_index;
        return candidate;
    }
    return NULL;
}

static bool runtime_emissive_direct_3d_accumulate_triangle(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t base_seed,
    int triangle_index,
    double emissive,
    double base_color_r,
    double base_color_g,
    double base_color_b,
    double area,
    double candidate_selection_pdf,
    RuntimeEmissiveDirect3DResult* io_result) {
    const RuntimeTriangle3D* triangle = NULL;
    Vec3 sample_point = vec3(0.0, 0.0, 0.0);
    Vec3 to_emitter = vec3(0.0, 0.0, 0.0);
    Vec3 light_dir = vec3(0.0, 0.0, 0.0);
    double distance = 0.0;
    double receiver_facing = 0.0;
    double emitter_facing = 0.0;
    double transmittance = 0.0;
    double sample_energy = 0.0;
    double light_pdf = 0.0;

    if (!scene || !hit || !io_result ||
        !scene->triangleMesh.triangles ||
        triangle_index < 0 ||
        triangle_index >= scene->triangleMesh.triangleCount ||
        !(emissive > 0.0)) {
        return false;
    }

    triangle = &scene->triangleMesh.triangles[triangle_index];
    if (triangle->sceneObjectIndex < 0 ||
        triangle->sceneObjectIndex == hit->sceneObjectIndex) {
        return false;
    }

    sample_point = runtime_emissive_direct_3d_sample_triangle_point(triangle,
                                                                    sampling,
                                                                    base_seed,
                                                                    triangle_index);
    to_emitter = vec3_sub(sample_point, hit->position);
    distance = vec3_length(to_emitter);
    if (!(distance > kRuntimeEmissiveDirect3DEpsilon)) {
        return false;
    }

    light_dir = vec3_scale(to_emitter, 1.0 / distance);
    receiver_facing = fmax(0.0, vec3_dot(hit->normal, light_dir));
    emitter_facing = fmax(0.0, vec3_dot(triangle->normal, vec3_scale(light_dir, -1.0)));
    if (!(receiver_facing > 0.0) || !(emitter_facing > 0.0)) {
        return false;
    }
    light_pdf = runtime_emissive_direct_3d_solid_angle_pdf(candidate_selection_pdf,
                                                           area,
                                                           distance,
                                                           emitter_facing);

    io_result->sampledTriangleCount += 1;
    io_result->visibilityRayCount += 1;
    transmittance = RuntimeVisibility3D_TransmittanceFromHitToPoint(scene,
                                                                    hit,
                                                                    sample_point,
                                                                    triangle->sceneObjectIndex,
                                                                    triangle_index);
    if (!(transmittance > 1e-6)) {
        return false;
    }

    sample_energy = emissive *
                    receiver_facing *
                    emitter_facing *
                    transmittance *
                    runtime_emissive_direct_3d_distance_decay(distance) *
                    runtime_emissive_direct_3d_clamp(area, 0.25, 4.0) *
                    kRuntimeEmissiveDirect3DEnergyScale;
    if (!(sample_energy > 0.0)) {
        return false;
    }

    io_result->contributingTriangleCount += 1;
    io_result->sampleDirection = light_dir;
    io_result->sampleDistance = distance;
    io_result->sampleArea = area;
    io_result->sampleReceiverCos = receiver_facing;
    io_result->sampleEmitterCos = emitter_facing;
    io_result->candidateSelectionPdf = candidate_selection_pdf;
    io_result->areaPdf = area > 1e-12 ? 1.0 / area : 0.0;
    io_result->lightPdf = light_pdf;
    io_result->directRadiance += sample_energy;
    io_result->directRadianceR += sample_energy * base_color_r;
    io_result->directRadianceG += sample_energy * base_color_g;
    io_result->directRadianceB += sample_energy * base_color_b;
    return true;
}

static bool runtime_emissive_direct_3d_shade_hit_with_light_set(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t base_seed,
    RuntimeEmissiveDirect3DResult* io_result) {
    bool contributed = false;

    if (!scene || !hit || !io_result || !scene->emissiveLightSet.valid) {
        return false;
    }
    io_result->candidateCount = scene->emissiveLightSet.candidateCount;
    if (scene->emissiveLightSet.candidateCount <= 0) {
        return false;
    }

    for (int sample_index = 0;
         sample_index < kRuntimeEmissiveDirect3DMaxCandidateSamples;
         ++sample_index) {
        int candidate_index = -1;
        double candidate_selection_pdf = 0.0;
        const RuntimeEmissiveLightCandidate3D* candidate =
            runtime_emissive_direct_3d_select_candidate(scene,
                                                        hit,
                                                        sampling,
                                                        base_seed,
                                                        sample_index,
                                                        &candidate_index);
        (void)candidate_index;
        if (!candidate) {
            continue;
        }
        io_result->selectedCandidateCount += 1;
        if (scene->emissiveLightSet.totalWeight > 1e-12 && candidate->weight > 0.0) {
            candidate_selection_pdf = runtime_emissive_direct_3d_clamp(
                candidate->weight / scene->emissiveLightSet.totalWeight,
                0.0,
                1.0);
        }
        if (runtime_emissive_direct_3d_accumulate_triangle(scene,
                                                           hit,
                                                           sampling,
                                                           base_seed,
                                                           candidate->triangleIndex,
                                                           candidate->emissive,
                                                           candidate->baseColorR,
                                                           candidate->baseColorG,
                                                           candidate->baseColorB,
                                                           candidate->area,
                                                           candidate_selection_pdf,
                                                           io_result)) {
            contributed = true;
        }
    }
    return contributed;
}

static bool runtime_emissive_direct_3d_shade_hit_full_scan_fallback(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t base_seed,
    RuntimeEmissiveDirect3DResult* io_result) {
    bool contributed = false;

    if (!scene || !hit || !io_result ||
        !scene->triangleMesh.triangles ||
        scene->triangleMesh.triangleCount <= 0) {
        return false;
    }

    io_result->fullScanFallbackCount += 1;
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        RuntimeMaterialPayload3D payload = {0};
        double area = 0.0;

        if (triangle->sceneObjectIndex < 0 ||
            triangle->sceneObjectIndex == hit->sceneObjectIndex) {
            continue;
        }
        if (!RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(triangle->sceneObjectIndex,
                                                                  &payload) ||
            !(payload.emissive > 0.0)) {
            continue;
        }
        area = runtime_emissive_direct_3d_triangle_area(triangle);
        if (runtime_emissive_direct_3d_accumulate_triangle(scene,
                                                           hit,
                                                           sampling,
                                                           base_seed,
                                                           i,
                                                           payload.emissive,
                                                           payload.baseColorR,
                                                           payload.baseColorG,
                                                           payload.baseColorB,
                                                           area,
                                                           1.0,
                                                           io_result)) {
            contributed = true;
        }
    }
    return contributed;
}

bool RuntimeEmissiveDirect3D_ShadeHit(const RuntimeScene3D* scene,
                                      const HitInfo3D* hit,
                                      const RuntimeNative3DSamplingContext* sampling,
                                      RuntimeEmissiveDirect3DResult* out_result) {
    RuntimeEmissiveDirect3DResult result = {0};
    uint32_t base_seed = 0U;
    bool contributed = false;

    if (!scene || !hit || !out_result) return false;
    if (scene->emissiveLightSet.valid) {
        result.candidateCount = scene->emissiveLightSet.candidateCount;
    }
    if (scene->capabilities.valid && scene->capabilities.canSkipEmissionSupport) {
        *out_result = result;
        return false;
    }
    if (!scene->triangleMesh.triangles || scene->triangleMesh.triangleCount <= 0) {
        *out_result = result;
        return false;
    }

    base_seed = runtime_emissive_direct_3d_seed_from_hit(hit, sampling);
    if (scene->emissiveLightSet.valid) {
        contributed = runtime_emissive_direct_3d_shade_hit_with_light_set(scene,
                                                                          hit,
                                                                          sampling,
                                                                          base_seed,
                                                                          &result);
    } else {
        contributed = runtime_emissive_direct_3d_shade_hit_full_scan_fallback(scene,
                                                                              hit,
                                                                              sampling,
                                                                              base_seed,
                                                                              &result);
    }

    *out_result = result;
    return contributed && result.contributingTriangleCount > 0;
}
