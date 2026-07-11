#include "render/runtime_caustic_photon_emit_3d.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    RUNTIME_CAUSTIC_PHOTON_EMISSION_DEFAULT_BUDGET = 256,
    RUNTIME_CAUSTIC_PHOTON_EMISSION_MAX_BUDGET = 262144
};

static double photon_emit_luma(Vec3 value) {
    return 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
}

static Vec3 photon_emit_add(Vec3 a, Vec3 b) {
    return vec3_add(a, b);
}

static bool photon_emit_vec3_isfinite(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static Vec3 photon_emit_normalize_or(Vec3 value, Vec3 fallback) {
    if (!photon_emit_vec3_isfinite(value) || !(vec3_length(value) > 1.0e-12)) {
        return fallback;
    }
    return vec3_normalize(value);
}

static uint32_t photon_emit_hash_u32(uint64_t value) {
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return (uint32_t)(value & UINT32_C(0xffffffff));
}

static double photon_emit_unit(uint32_t seed) {
    return ((double)seed + 0.5) / 4294967296.0;
}

static double photon_emit_nonnegative(double value) {
    if (!isfinite(value) || value < 0.0) return 0.0;
    return value;
}

static double photon_emit_light_area(const RuntimeLightSource3D* light) {
    if (!light) return 0.0;
    switch (light->kind) {
        case RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE:
            return 4.0 * M_PI * photon_emit_nonnegative(light->radius) *
                   photon_emit_nonnegative(light->radius);
        case RUNTIME_LIGHT_SOURCE_3D_KIND_DISK:
            return M_PI * photon_emit_nonnegative(light->radius) *
                   photon_emit_nonnegative(light->radius);
        case RUNTIME_LIGHT_SOURCE_3D_KIND_RECT:
            return photon_emit_nonnegative(light->width) *
                   photon_emit_nonnegative(light->height);
        case RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE:
            return light->emissiveArea > 0.0 ? light->emissiveArea : 1.0;
        case RUNTIME_LIGHT_SOURCE_3D_KIND_POINT:
        default:
            return 1.0;
    }
}

static double photon_emit_light_weight(const RuntimeLightSource3D* light) {
    double color_luma = 0.0;
    if (!light || !light->enabled) return 0.0;
    color_luma = photon_emit_luma(light->color);
    if (!(color_luma > 0.0)) color_luma = 1.0;
    return photon_emit_nonnegative(light->intensity) *
           color_luma *
           photon_emit_light_area(light);
}

static const RuntimeLightSource3D* photon_emit_pick_enabled_light(
    const RuntimeLightSet3D* light_set,
    double target_weight,
    double* out_light_weight,
    int* out_light_index) {
    double accum = 0.0;
    int enabled_index = 0;
    if (out_light_weight) *out_light_weight = 0.0;
    if (out_light_index) *out_light_index = -1;
    if (!light_set) return NULL;
    for (int i = 0; i < light_set->lightCount; ++i) {
        const RuntimeLightSource3D* light = &light_set->lights[i];
        const double weight = photon_emit_light_weight(light);
        if (!(weight > 0.0)) continue;
        accum += weight;
        if (target_weight <= accum) {
            if (out_light_weight) *out_light_weight = weight;
            if (out_light_index) *out_light_index = enabled_index;
            return light;
        }
        ++enabled_index;
    }
    return NULL;
}

static Vec3 photon_emit_sample_unit_sphere(double u0, double u1) {
    const double z = 1.0 - 2.0 * u0;
    const double r = sqrt(fmax(0.0, 1.0 - z * z));
    const double phi = 2.0 * M_PI * u1;
    return vec3(r * cos(phi), z, r * sin(phi));
}

static Vec3 photon_emit_sample_position(const RuntimeLightSource3D* light,
                                        double u0,
                                        double u1) {
    Vec3 axis_u;
    Vec3 axis_v;
    Vec3 normal;
    double radius = 0.0;
    double r = 0.0;
    double phi = 0.0;
    if (!light) return vec3(0.0, 0.0, 0.0);

    axis_u = photon_emit_normalize_or(light->axisU, vec3(1.0, 0.0, 0.0));
    axis_v = photon_emit_normalize_or(light->axisV, vec3(0.0, 0.0, 1.0));
    normal = photon_emit_normalize_or(light->normal, vec3(0.0, -1.0, 0.0));

    switch (light->kind) {
        case RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE:
            radius = photon_emit_nonnegative(light->radius);
            return vec3_add(light->position,
                            vec3_scale(photon_emit_sample_unit_sphere(u0, u1), radius));
        case RUNTIME_LIGHT_SOURCE_3D_KIND_DISK:
            radius = photon_emit_nonnegative(light->radius);
            r = sqrt(u0) * radius;
            phi = 2.0 * M_PI * u1;
            return vec3_add(light->position,
                            vec3_add(vec3_scale(axis_u, cos(phi) * r),
                                     vec3_scale(axis_v, sin(phi) * r)));
        case RUNTIME_LIGHT_SOURCE_3D_KIND_RECT:
            return vec3_add(light->position,
                            vec3_add(vec3_scale(axis_u, (u0 - 0.5) * light->width),
                                     vec3_scale(axis_v, (u1 - 0.5) * light->height)));
        case RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE:
            return light->emissiveArea > 0.0 ? light->emissiveCentroid : light->position;
        case RUNTIME_LIGHT_SOURCE_3D_KIND_POINT:
        default:
            (void)normal;
            return light->position;
    }
}

static Vec3 photon_emit_sample_direction(const RuntimeLightSource3D* light,
                                         Vec3 sampled_position,
                                         double u0,
                                         double u1) {
    Vec3 normal;
    if (!light) return vec3(0.0, -1.0, 0.0);
    normal = photon_emit_normalize_or(light->normal, vec3(0.0, -1.0, 0.0));
    if (light->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE) {
        return photon_emit_normalize_or(vec3_sub(sampled_position, light->position),
                                        normal);
    }
    if (light->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI) {
        return photon_emit_sample_unit_sphere(u0, u1);
    }
    if (light->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED &&
        u0 < 0.5) {
        return vec3_scale(normal, -1.0);
    }
    return normal;
}

static void photon_emit_diag_store(RuntimeCausticPhotonEmissionBatch3D* batch,
                                   RuntimeCausticPhotonEmissionDiagnostics3D diag,
                                   RuntimeCausticPhotonEmissionDiagnostics3D* out) {
    if (batch) batch->diagnostics = diag;
    if (out) *out = diag;
}

void RuntimeCausticPhotonEmission3D_DefaultSettings(
    RuntimeCausticPhotonEmissionSettings3D* settings) {
    if (!settings) return;
    memset(settings, 0, sizeof(*settings));
    settings->sampleBudget = RUNTIME_CAUSTIC_PHOTON_EMISSION_DEFAULT_BUDGET;
    settings->baseSeed = UINT32_C(0x9e3779b9);
    settings->firstPhotonId = 1u;
    settings->defaultQueryRadius = 0.10;
}

void RuntimeCausticPhotonEmission3D_InitBatch(
    RuntimeCausticPhotonEmissionBatch3D* batch) {
    if (!batch) return;
    memset(batch, 0, sizeof(*batch));
}

bool RuntimeCausticPhotonEmission3D_AllocateBatch(
    RuntimeCausticPhotonEmissionBatch3D* batch,
    uint64_t sample_capacity) {
    RuntimeCausticPhotonEmissionBatch3D allocated;
    if (!batch) return false;
    if (sample_capacity == 0u) {
        sample_capacity = RUNTIME_CAUSTIC_PHOTON_EMISSION_DEFAULT_BUDGET;
    }
    if (sample_capacity > RUNTIME_CAUSTIC_PHOTON_EMISSION_MAX_BUDGET) {
        sample_capacity = RUNTIME_CAUSTIC_PHOTON_EMISSION_MAX_BUDGET;
    }
    if (sample_capacity >
        (uint64_t)(SIZE_MAX / sizeof(RuntimeCausticPhotonSample3D))) {
        return false;
    }
    RuntimeCausticPhotonEmission3D_InitBatch(&allocated);
    allocated.samples = (RuntimeCausticPhotonSample3D*)calloc(
        (size_t)sample_capacity,
        sizeof(RuntimeCausticPhotonSample3D));
    if (!allocated.samples) return false;
    allocated.sampleCapacity = sample_capacity;
    allocated.ownsSamples = true;
    RuntimeCausticPhotonEmission3D_FreeBatch(batch);
    *batch = allocated;
    return true;
}

void RuntimeCausticPhotonEmission3D_ClearBatch(
    RuntimeCausticPhotonEmissionBatch3D* batch) {
    if (!batch) return;
    if (batch->samples && batch->sampleCapacity > 0u) {
        memset(batch->samples,
               0,
               (size_t)batch->sampleCapacity * sizeof(RuntimeCausticPhotonSample3D));
    }
    batch->sampleCount = 0u;
    memset(&batch->diagnostics, 0, sizeof(batch->diagnostics));
}

void RuntimeCausticPhotonEmission3D_FreeBatch(
    RuntimeCausticPhotonEmissionBatch3D* batch) {
    if (!batch) return;
    free(batch->samples);
    RuntimeCausticPhotonEmission3D_InitBatch(batch);
}

bool RuntimeCausticPhotonEmission3D_EmitFromLightSet(
    RuntimeCausticPhotonEmissionBatch3D* batch,
    const RuntimeLightSet3D* light_set,
    const RuntimeCausticPhotonEmissionSettings3D* settings,
    RuntimeCausticPhotonEmissionDiagnostics3D* out_diagnostics) {
    RuntimeCausticPhotonEmissionSettings3D defaults;
    const RuntimeCausticPhotonEmissionSettings3D* active = settings;
    RuntimeCausticPhotonEmissionDiagnostics3D diag;
    double total_weight = 0.0;
    uint64_t requested = 0u;

    memset(&diag, 0, sizeof(diag));
    if (!active) {
        RuntimeCausticPhotonEmission3D_DefaultSettings(&defaults);
        active = &defaults;
    }
    requested = active->sampleBudget;
    if (requested > RUNTIME_CAUSTIC_PHOTON_EMISSION_MAX_BUDGET) {
        requested = RUNTIME_CAUSTIC_PHOTON_EMISSION_MAX_BUDGET;
    }
    diag.requestedSampleBudget = requested;

    if (!batch || !batch->samples || batch->sampleCapacity == 0u || requested == 0u) {
        diag.lastRejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_MAP_CAPACITY;
        diag.rejectedPhotonCount = requested;
        photon_emit_diag_store(batch, diag, out_diagnostics);
        return false;
    }
    RuntimeCausticPhotonEmission3D_ClearBatch(batch);

    diag.sourceCount = light_set && light_set->lightCount > 0
                           ? (uint64_t)light_set->lightCount
                           : 0u;
    if (light_set) {
        for (int i = 0; i < light_set->lightCount; ++i) {
            const double weight = photon_emit_light_weight(&light_set->lights[i]);
            if (weight > 0.0) {
                total_weight += weight;
                diag.enabledSourceCount += 1u;
            }
        }
    }
    if (!(total_weight > 0.0)) {
        diag.lastRejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_ESCAPED_SCENE;
        diag.rejectedPhotonCount = requested;
        photon_emit_diag_store(batch, diag, out_diagnostics);
        return false;
    }

    for (uint64_t i = 0u; i < requested; ++i) {
        RuntimeCausticPhotonSample3D sample;
        const uint32_t seed0 = photon_emit_hash_u32(
            ((uint64_t)active->baseSeed << 32) ^ i);
        const uint32_t seed1 = photon_emit_hash_u32(
            ((uint64_t)active->baseSeed << 32) ^ i ^ UINT64_C(0x51ed2705));
        const uint32_t seed2 = photon_emit_hash_u32(
            ((uint64_t)active->baseSeed << 32) ^ i ^ UINT64_C(0xa24baed4));
        const uint32_t seed3 = photon_emit_hash_u32(
            ((uint64_t)active->baseSeed << 32) ^ i ^ UINT64_C(0x3c6ef372));
        const double source_u = photon_emit_unit(seed0);
        const double source_target = source_u * total_weight;
        double light_weight = 0.0;
        int light_index = -1;
        const RuntimeLightSource3D* light =
            photon_emit_pick_enabled_light(light_set,
                                           source_target,
                                           &light_weight,
                                           &light_index);
        const double source_pdf = light_weight > 0.0 ? light_weight / total_weight : 0.0;
        if (batch->sampleCount >= batch->sampleCapacity || !light ||
            !(source_pdf > 1.0e-12)) {
            diag.rejectedPhotonCount += 1u;
            diag.lastRejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_MAP_CAPACITY;
            continue;
        }

        memset(&sample, 0, sizeof(sample));
        sample.photonId = active->firstPhotonId + i;
        sample.sampleIndex = i;
        sample.rngSeed = seed0;
        sample.lightIndex = light_index;
        sample.wavelengthBucket = (int)(seed1 % 3u);
        sample.position = photon_emit_sample_position(light,
                                                      photon_emit_unit(seed1),
                                                      photon_emit_unit(seed2));
        sample.direction = photon_emit_sample_direction(light,
                                                       sample.position,
                                                       photon_emit_unit(seed2),
                                                       photon_emit_unit(seed3));
        sample.emissionPdf = source_pdf;
        sample.flux = vec3_scale(light->color,
                                 photon_emit_nonnegative(light->intensity) /
                                     (source_pdf * (double)requested));
        batch->samples[batch->sampleCount++] = sample;
        diag.emittedPhotonCount += 1u;
        diag.sourcePdfSum += source_pdf;
        diag.totalEmittedFlux =
            photon_emit_add(diag.totalEmittedFlux, sample.flux);
        if (diag.emittedPhotonCount == 1u) diag.firstSeed = seed0;
        diag.lastSeed = seed0;
    }

    photon_emit_diag_store(batch, diag, out_diagnostics);
    return diag.emittedPhotonCount > 0u;
}

bool RuntimeCausticPhotonEmission3D_StoreSurfaceProxyRecords(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonEmissionBatch3D* batch,
    double query_radius,
    Vec3 receiver_normal,
    int receiver_scene_object_index,
    int receiver_primitive_index,
    int receiver_triangle_index,
    RuntimeCausticPhotonEmissionDiagnostics3D* out_diagnostics) {
    RuntimeCausticPhotonEmissionDiagnostics3D diag;
    Vec3 normal = photon_emit_normalize_or(receiver_normal, vec3(0.0, 1.0, 0.0));
    const double active_query_radius =
        isfinite(query_radius) && query_radius > 0.0 ? query_radius : 0.10;
    bool stored_any = false;

    memset(&diag, 0, sizeof(diag));
    if (batch) {
        diag = batch->diagnostics;
    }
    if (!map || !batch || !batch->samples) {
        diag.lastRejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_MAP_CAPACITY;
        if (out_diagnostics) *out_diagnostics = diag;
        return false;
    }

    for (uint64_t i = 0u; i < batch->sampleCount; ++i) {
        RuntimeCausticPhotonMapRecord3D record;
        const RuntimeCausticPhotonSample3D* sample = &batch->samples[i];
        memset(&record, 0, sizeof(record));
        record.photonId = sample->photonId;
        record.depth = 0u;
        record.position = sample->position;
        record.normal = normal;
        record.incidentDirection = sample->direction;
        record.flux = sample->flux;
        record.pathPdf = sample->emissionPdf;
        record.queryRadius = active_query_radius;
        record.sceneObjectIndex = receiver_scene_object_index;
        record.primitiveIndex = receiver_primitive_index;
        record.triangleIndex = receiver_triangle_index;
        diag.mapStoreAttemptCount += 1u;
        if (RuntimeCausticPhotonMap3D_StoreRecord(map, &record)) {
            diag.mapStoreAcceptedCount += 1u;
            diag.totalStoredSurfaceFlux =
                photon_emit_add(diag.totalStoredSurfaceFlux, record.flux);
            stored_any = true;
        } else {
            diag.mapStoreRejectedCount += 1u;
            diag.lastRejectReason = RUNTIME_CAUSTIC_PHOTON_REJECT_MAP_CAPACITY;
        }
    }

    if (out_diagnostics) *out_diagnostics = diag;
    return stored_any;
}
