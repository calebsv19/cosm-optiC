#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>
#include <string.h>

#include "material/material.h"

static void runtime_caustic_transport_vec3_minmax(Vec3 p, Vec3* io_min, Vec3* io_max) {
    if (!io_min || !io_max) return;
    if (p.x < io_min->x) io_min->x = p.x;
    if (p.y < io_min->y) io_min->y = p.y;
    if (p.z < io_min->z) io_min->z = p.z;
    if (p.x > io_max->x) io_max->x = p.x;
    if (p.y > io_max->y) io_max->y = p.y;
    if (p.z > io_max->z) io_max->z = p.z;
}

bool runtime_caustic_transport_payload_is_eligible(
    const RuntimeMaterialPayload3D* payload) {
    if (!payload || !payload->valid) return false;
    if (payload->materialId == MATERIAL_PRESET_TRANSPARENT) return true;
    if (payload->transparency > 1.0e-6) return true;
    if (payload->opticalIor > 1.0001 || payload->bsdf.ior > 1.0001) return true;
    return payload->bsdf.reflectivity > 0.10 && payload->bsdf.roughness <= 0.35;
}

bool runtime_caustic_transport_resolve_analytic_sphere(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportAnalyticSphere3D* out_sphere) {
    typedef struct {
        bool seen;
        int sceneObjectIndex;
        int primitiveIndex;
        int triangleCount;
        Vec3 boundsMin;
        Vec3 boundsMax;
        RuntimeMaterialPayload3D payload;
    } Candidate;

    Candidate candidates[MAX_OBJECTS];
    RuntimeCausticTransportAnalyticSphere3D best;
    double best_score = -1.0;

    if (out_sphere) memset(out_sphere, 0, sizeof(*out_sphere));
    if (!scene || !out_sphere) return false;
    memset(candidates, 0, sizeof(candidates));
    memset(&best, 0, sizeof(best));

    for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[tri_i];
        HitInfo3D hit = {0};
        RuntimeMaterialPayload3D payload = {0};
        Candidate* candidate = NULL;
        Vec3 centroid = vec3_scale(vec3_add(vec3_add(triangle->p0, triangle->p1),
                                            triangle->p2),
                                   1.0 / 3.0);
        int object_index = triangle->sceneObjectIndex;

        if (object_index < 0 || object_index >= MAX_OBJECTS) continue;
        HitInfo3D_Reset(&hit);
        hit.t = 0.0;
        hit.position = centroid;
        hit.normal = triangle->normal;
        hit.triangleIndex = tri_i;
        hit.primitiveIndex = triangle->primitiveIndex;
        hit.sceneObjectIndex = object_index;
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload) ||
            !runtime_caustic_transport_payload_is_eligible(&payload)) {
            continue;
        }

        candidate = &candidates[object_index];
        if (!candidate->seen) {
            candidate->seen = true;
            candidate->sceneObjectIndex = object_index;
            candidate->primitiveIndex = triangle->primitiveIndex;
            candidate->boundsMin = triangle->p0;
            candidate->boundsMax = triangle->p0;
            candidate->payload = payload;
        }
        candidate->triangleCount += 1;
        runtime_caustic_transport_vec3_minmax(triangle->p0,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p1,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p2,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
    }

    for (int i = 0; i < MAX_OBJECTS; ++i) {
        Candidate* candidate = &candidates[i];
        RuntimeCausticTransportAnalyticSphere3D resolved;
        Vec3 extent = vec3(0.0, 0.0, 0.0);
        double half_x = 0.0;
        double half_y = 0.0;
        double half_z = 0.0;
        double min_half = 0.0;
        double max_half = 0.0;
        double radius = 0.0;
        double roundness = 0.0;
        double score = 0.0;
        if (!candidate->seen || candidate->triangleCount < 6) continue;
        extent = vec3_sub(candidate->boundsMax, candidate->boundsMin);
        half_x = extent.x * 0.5;
        half_y = extent.y * 0.5;
        half_z = extent.z * 0.5;
        min_half = fmin(half_x, fmin(half_y, half_z));
        max_half = fmax(half_x, fmax(half_y, half_z));
        if (!(min_half > 1.0e-6) || !(max_half > 1.0e-6)) continue;
        roundness = max_half / min_half;
        if (roundness > 1.35) continue;
        radius = (half_x + half_y + half_z) / 3.0;
        if (!(radius > 1.0e-6)) continue;

        memset(&resolved, 0, sizeof(resolved));
        resolved.valid = true;
        resolved.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.primitiveIndex = candidate->primitiveIndex;
        resolved.triangleCount = candidate->triangleCount;
        RuntimeCausticSphereLens3D_DefaultDescriptor(&resolved.sphere);
        resolved.sphere.center = vec3_scale(vec3_add(candidate->boundsMin,
                                                     candidate->boundsMax),
                                            0.5);
        resolved.sphere.radius = radius;
        resolved.sphere.ior =
            candidate->payload.opticalIor > 1.0001
                ? candidate->payload.opticalIor
                : fmax(candidate->payload.bsdf.ior, 1.5);
        resolved.sphere.tint = vec3(candidate->payload.baseColorR,
                                    candidate->payload.baseColorG,
                                    candidate->payload.baseColorB);
        resolved.sphere.absorptionDistance = candidate->payload.absorptionDistance;
        resolved.payload = candidate->payload;

        score = (double)candidate->triangleCount / roundness;
        if (score > best_score) {
            best_score = score;
            best = resolved;
        }
    }

    if (!best.valid) return false;
    *out_sphere = best;
    return true;
}

bool runtime_caustic_transport_resolve_analytic_cylinder(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportAnalyticCylinder3D* out_cylinder) {
    typedef struct {
        bool seen;
        int sceneObjectIndex;
        int primitiveIndex;
        int triangleCount;
        Vec3 boundsMin;
        Vec3 boundsMax;
        RuntimeMaterialPayload3D payload;
    } Candidate;

    Candidate candidates[MAX_OBJECTS];
    RuntimeCausticTransportAnalyticCylinder3D best;
    double best_score = -1.0;

    if (out_cylinder) memset(out_cylinder, 0, sizeof(*out_cylinder));
    if (!scene || !out_cylinder) return false;
    memset(candidates, 0, sizeof(candidates));
    memset(&best, 0, sizeof(best));

    for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[tri_i];
        HitInfo3D hit = {0};
        RuntimeMaterialPayload3D payload = {0};
        Candidate* candidate = NULL;
        Vec3 centroid = vec3_scale(vec3_add(vec3_add(triangle->p0, triangle->p1),
                                            triangle->p2),
                                   1.0 / 3.0);
        int object_index = triangle->sceneObjectIndex;

        if (object_index < 0 || object_index >= MAX_OBJECTS) continue;
        HitInfo3D_Reset(&hit);
        hit.t = 0.0;
        hit.position = centroid;
        hit.normal = triangle->normal;
        hit.triangleIndex = tri_i;
        hit.primitiveIndex = triangle->primitiveIndex;
        hit.sceneObjectIndex = object_index;
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload) ||
            !runtime_caustic_transport_payload_is_eligible(&payload)) {
            continue;
        }

        candidate = &candidates[object_index];
        if (!candidate->seen) {
            candidate->seen = true;
            candidate->sceneObjectIndex = object_index;
            candidate->primitiveIndex = triangle->primitiveIndex;
            candidate->boundsMin = triangle->p0;
            candidate->boundsMax = triangle->p0;
            candidate->payload = payload;
        }
        candidate->triangleCount += 1;
        runtime_caustic_transport_vec3_minmax(triangle->p0,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p1,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p2,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
    }

    for (int i = 0; i < MAX_OBJECTS; ++i) {
        Candidate* candidate = &candidates[i];
        RuntimeCausticTransportAnalyticCylinder3D resolved;
        Vec3 extent = vec3(0.0, 0.0, 0.0);
        Vec3 axis = vec3(0.0, 0.0, 1.0);
        double halves[3] = {0.0, 0.0, 0.0};
        int long_axis = 2;
        int a = 0;
        int b = 1;
        double long_half = 0.0;
        double r0 = 0.0;
        double r1 = 0.0;
        double radial_min = 0.0;
        double radial_max = 0.0;
        double radius = 0.0;
        double radial_roundness = 0.0;
        double elongation = 0.0;
        double score = 0.0;
        if (!candidate->seen || candidate->triangleCount < 6) continue;
        extent = vec3_sub(candidate->boundsMax, candidate->boundsMin);
        halves[0] = extent.x * 0.5;
        halves[1] = extent.y * 0.5;
        halves[2] = extent.z * 0.5;
        if (halves[0] > halves[long_axis]) long_axis = 0;
        if (halves[1] > halves[long_axis]) long_axis = 1;
        if (long_axis == 0) {
            a = 1;
            b = 2;
            axis = vec3(1.0, 0.0, 0.0);
        } else if (long_axis == 1) {
            a = 0;
            b = 2;
            axis = vec3(0.0, 1.0, 0.0);
        }
        long_half = halves[long_axis];
        r0 = halves[a];
        r1 = halves[b];
        radial_min = fmin(r0, r1);
        radial_max = fmax(r0, r1);
        if (!(radial_min > 1.0e-6) || !(long_half > 1.0e-6)) continue;
        radial_roundness = radial_max / radial_min;
        elongation = long_half / radial_max;
        if (radial_roundness > 1.45 || elongation < 1.35) continue;
        radius = (r0 + r1) * 0.5;
        if (!(radius > 1.0e-6)) continue;

        memset(&resolved, 0, sizeof(resolved));
        resolved.valid = true;
        resolved.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.primitiveIndex = candidate->primitiveIndex;
        resolved.triangleCount = candidate->triangleCount;
        RuntimeCausticLensTransport3D_DefaultShape(&resolved.shape);
        resolved.shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_CYLINDER;
        resolved.shape.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.shape.primitiveIndex = candidate->primitiveIndex;
        resolved.shape.boundsMin = candidate->boundsMin;
        resolved.shape.boundsMax = candidate->boundsMax;
        resolved.shape.center = vec3_scale(vec3_add(candidate->boundsMin,
                                                    candidate->boundsMax),
                                           0.5);
        resolved.shape.axis = axis;
        resolved.shape.radius = radius;
        resolved.shape.height = long_half * 2.0;
        resolved.shape.payload = candidate->payload;
        resolved.payload = candidate->payload;

        score = (double)candidate->triangleCount * elongation / radial_roundness;
        if (score > best_score) {
            best_score = score;
            best = resolved;
        }
    }

    if (!best.valid) return false;
    *out_cylinder = best;
    return true;
}

bool runtime_caustic_transport_resolve_analytic_prism(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportAnalyticPrism3D* out_prism) {
    typedef struct {
        bool seen;
        int sceneObjectIndex;
        int primitiveIndex;
        int triangleCount;
        Vec3 boundsMin;
        Vec3 boundsMax;
        RuntimeMaterialPayload3D payload;
    } Candidate;

    Candidate candidates[MAX_OBJECTS];
    RuntimeCausticTransportAnalyticPrism3D best;
    double best_score = -1.0;

    if (out_prism) memset(out_prism, 0, sizeof(*out_prism));
    if (!scene || !out_prism) return false;
    memset(candidates, 0, sizeof(candidates));
    memset(&best, 0, sizeof(best));

    for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[tri_i];
        HitInfo3D hit = {0};
        RuntimeMaterialPayload3D payload = {0};
        Candidate* candidate = NULL;
        Vec3 centroid = vec3_scale(vec3_add(vec3_add(triangle->p0, triangle->p1),
                                            triangle->p2),
                                   1.0 / 3.0);
        int object_index = triangle->sceneObjectIndex;

        if (object_index < 0 || object_index >= MAX_OBJECTS) continue;
        HitInfo3D_Reset(&hit);
        hit.t = 0.0;
        hit.position = centroid;
        hit.normal = triangle->normal;
        hit.triangleIndex = tri_i;
        hit.primitiveIndex = triangle->primitiveIndex;
        hit.sceneObjectIndex = object_index;
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload) ||
            !runtime_caustic_transport_payload_is_eligible(&payload)) {
            continue;
        }

        candidate = &candidates[object_index];
        if (!candidate->seen) {
            candidate->seen = true;
            candidate->sceneObjectIndex = object_index;
            candidate->primitiveIndex = triangle->primitiveIndex;
            candidate->boundsMin = triangle->p0;
            candidate->boundsMax = triangle->p0;
            candidate->payload = payload;
        }
        candidate->triangleCount += 1;
        runtime_caustic_transport_vec3_minmax(triangle->p0,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p1,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p2,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
    }

    for (int i = 0; i < MAX_OBJECTS; ++i) {
        Candidate* candidate = &candidates[i];
        RuntimeCausticTransportAnalyticPrism3D resolved;
        Vec3 extent = vec3(0.0, 0.0, 0.0);
        Vec3 axis = vec3(0.0, 0.0, 1.0);
        double halves[3] = {0.0, 0.0, 0.0};
        int long_axis = 2;
        int a = 0;
        int b = 1;
        double long_half = 0.0;
        double r0 = 0.0;
        double r1 = 0.0;
        double radius = 0.0;
        double score = 0.0;
        if (!candidate->seen || candidate->triangleCount < 4) continue;
        extent = vec3_sub(candidate->boundsMax, candidate->boundsMin);
        halves[0] = extent.x * 0.5;
        halves[1] = extent.y * 0.5;
        halves[2] = extent.z * 0.5;
        if (halves[0] > halves[long_axis]) long_axis = 0;
        if (halves[1] > halves[long_axis]) long_axis = 1;
        if (long_axis == 0) {
            a = 1;
            b = 2;
            axis = vec3(1.0, 0.0, 0.0);
        } else if (long_axis == 1) {
            a = 0;
            b = 2;
            axis = vec3(0.0, 1.0, 0.0);
        }
        long_half = halves[long_axis];
        r0 = halves[a];
        r1 = halves[b];
        radius = fmax(r0, r1);
        if (!(long_half > 1.0e-6) || !(radius > 1.0e-6)) continue;

        memset(&resolved, 0, sizeof(resolved));
        resolved.valid = true;
        resolved.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.primitiveIndex = candidate->primitiveIndex;
        resolved.triangleCount = candidate->triangleCount;
        RuntimeCausticLensTransport3D_DefaultShape(&resolved.shape);
        resolved.shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_PRISM;
        resolved.shape.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.shape.primitiveIndex = candidate->primitiveIndex;
        resolved.shape.boundsMin = candidate->boundsMin;
        resolved.shape.boundsMax = candidate->boundsMax;
        resolved.shape.center = vec3_scale(vec3_add(candidate->boundsMin,
                                                    candidate->boundsMax),
                                           0.5);
        resolved.shape.axis = axis;
        resolved.shape.radius = radius;
        resolved.shape.height = long_half * 2.0;
        resolved.shape.payload = candidate->payload;
        resolved.payload = candidate->payload;

        score = (double)candidate->triangleCount * fmax(long_half / radius, 0.25);
        if (score > best_score) {
            best_score = score;
            best = resolved;
        }
    }

    if (!best.valid) return false;
    *out_prism = best;
    return true;
}

bool runtime_caustic_transport_resolve_analytic_bowl(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportAnalyticBowl3D* out_bowl) {
    typedef struct {
        bool seen;
        int sceneObjectIndex;
        int primitiveIndex;
        int triangleCount;
        Vec3 boundsMin;
        Vec3 boundsMax;
        RuntimeMaterialPayload3D payload;
    } Candidate;

    Candidate candidates[MAX_OBJECTS];
    RuntimeCausticTransportAnalyticBowl3D best;
    double best_score = -1.0;

    if (out_bowl) memset(out_bowl, 0, sizeof(*out_bowl));
    if (!scene || !out_bowl) return false;
    memset(candidates, 0, sizeof(candidates));
    memset(&best, 0, sizeof(best));

    for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[tri_i];
        HitInfo3D hit = {0};
        RuntimeMaterialPayload3D payload = {0};
        Candidate* candidate = NULL;
        Vec3 centroid = vec3_scale(vec3_add(vec3_add(triangle->p0, triangle->p1),
                                            triangle->p2),
                                   1.0 / 3.0);
        int object_index = triangle->sceneObjectIndex;

        if (object_index < 0 || object_index >= MAX_OBJECTS) continue;
        HitInfo3D_Reset(&hit);
        hit.t = 0.0;
        hit.position = centroid;
        hit.normal = triangle->normal;
        hit.triangleIndex = tri_i;
        hit.primitiveIndex = triangle->primitiveIndex;
        hit.sceneObjectIndex = object_index;
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload) ||
            !runtime_caustic_transport_payload_is_eligible(&payload)) {
            continue;
        }

        candidate = &candidates[object_index];
        if (!candidate->seen) {
            candidate->seen = true;
            candidate->sceneObjectIndex = object_index;
            candidate->primitiveIndex = triangle->primitiveIndex;
            candidate->boundsMin = triangle->p0;
            candidate->boundsMax = triangle->p0;
            candidate->payload = payload;
        }
        candidate->triangleCount += 1;
        runtime_caustic_transport_vec3_minmax(triangle->p0,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p1,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p2,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
    }

    for (int i = 0; i < MAX_OBJECTS; ++i) {
        Candidate* candidate = &candidates[i];
        RuntimeCausticTransportAnalyticBowl3D resolved;
        Vec3 extent = vec3(0.0, 0.0, 0.0);
        Vec3 axis = vec3(0.0, 0.0, 1.0);
        double halves[3] = {0.0, 0.0, 0.0};
        double min_half = 0.0;
        double max_half = 0.0;
        double mid_half = 0.0;
        double radius = 0.0;
        double thickness = 0.0;
        double flatness = 0.0;
        double roundness = 0.0;
        double score = 0.0;
        int thin_axis = 1;
        if (!candidate->seen || candidate->triangleCount < 6) continue;
        extent = vec3_sub(candidate->boundsMax, candidate->boundsMin);
        halves[0] = extent.x * 0.5;
        halves[1] = extent.y * 0.5;
        halves[2] = extent.z * 0.5;
        if (halves[0] < halves[thin_axis]) thin_axis = 0;
        if (halves[2] < halves[thin_axis]) thin_axis = 2;
        if (thin_axis == 0) {
            axis = vec3(1.0, 0.0, 0.0);
            min_half = halves[0];
            mid_half = fmin(halves[1], halves[2]);
            max_half = fmax(halves[1], halves[2]);
        } else if (thin_axis == 1) {
            axis = vec3(0.0, 1.0, 0.0);
            min_half = halves[1];
            mid_half = fmin(halves[0], halves[2]);
            max_half = fmax(halves[0], halves[2]);
        } else {
            axis = vec3(0.0, 0.0, 1.0);
            min_half = halves[2];
            mid_half = fmin(halves[0], halves[1]);
            max_half = fmax(halves[0], halves[1]);
        }
        if (!(min_half > 1.0e-6) || !(mid_half > 1.0e-6) || !(max_half > 1.0e-6)) {
            continue;
        }
        flatness = max_half / min_half;
        roundness = max_half / mid_half;
        if (flatness < 1.20 || roundness > 1.65) continue;
        radius = (mid_half + max_half) * 0.5;
        thickness = min_half * 2.0;
        if (!(radius > 1.0e-6) || !(thickness > 1.0e-6)) continue;

        memset(&resolved, 0, sizeof(resolved));
        resolved.valid = true;
        resolved.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.primitiveIndex = candidate->primitiveIndex;
        resolved.triangleCount = candidate->triangleCount;
        RuntimeCausticLensTransport3D_DefaultShape(&resolved.shape);
        resolved.shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_BOWL;
        resolved.shape.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.shape.primitiveIndex = candidate->primitiveIndex;
        resolved.shape.boundsMin = candidate->boundsMin;
        resolved.shape.boundsMax = candidate->boundsMax;
        resolved.shape.center = vec3_scale(vec3_add(candidate->boundsMin,
                                                    candidate->boundsMax),
                                           0.5);
        resolved.shape.axis = axis;
        resolved.shape.radius = radius;
        resolved.shape.height = thickness;
        resolved.shape.payload = candidate->payload;
        resolved.payload = candidate->payload;

        score = (double)candidate->triangleCount * flatness / roundness;
        if (score > best_score) {
            best_score = score;
            best = resolved;
        }
    }

    if (!best.valid) return false;
    *out_bowl = best;
    return true;
}

bool runtime_caustic_transport_resolve_mesh_dielectric(
    const RuntimeScene3D* scene,
    RuntimeCausticTransportMeshDielectric3D* out_mesh_dielectric) {
    typedef struct {
        bool seen;
        int sceneObjectIndex;
        int primitiveIndex;
        int triangleIndex;
        int triangleCount;
        RuntimeTriangle3D entryTriangle;
        Vec3 boundsMin;
        Vec3 boundsMax;
        RuntimeMaterialPayload3D payload;
        double areaScore;
    } Candidate;

    Candidate candidates[MAX_OBJECTS];
    RuntimeCausticTransportMeshDielectric3D best;
    double best_score = -1.0;

    if (out_mesh_dielectric) memset(out_mesh_dielectric, 0, sizeof(*out_mesh_dielectric));
    if (!scene || !out_mesh_dielectric) return false;
    memset(candidates, 0, sizeof(candidates));
    memset(&best, 0, sizeof(best));

    for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[tri_i];
        HitInfo3D hit = {0};
        RuntimeMaterialPayload3D payload = {0};
        Candidate* candidate = NULL;
        Vec3 centroid = vec3_scale(vec3_add(vec3_add(triangle->p0, triangle->p1),
                                            triangle->p2),
                                   1.0 / 3.0);
        Vec3 e0 = vec3_sub(triangle->p1, triangle->p0);
        Vec3 e1 = vec3_sub(triangle->p2, triangle->p0);
        double area = vec3_length(vec3_cross(e0, e1)) * 0.5;
        int object_index = triangle->sceneObjectIndex;

        if (object_index < 0 || object_index >= MAX_OBJECTS) continue;
        if (!(area > 1.0e-10)) continue;
        HitInfo3D_Reset(&hit);
        hit.t = 0.0;
        hit.position = centroid;
        hit.normal = triangle->normal;
        hit.triangleIndex = tri_i;
        hit.primitiveIndex = triangle->primitiveIndex;
        hit.sceneObjectIndex = object_index;
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload) ||
            !runtime_caustic_transport_payload_is_eligible(&payload)) {
            continue;
        }

        candidate = &candidates[object_index];
        if (!candidate->seen) {
            candidate->seen = true;
            candidate->sceneObjectIndex = object_index;
            candidate->primitiveIndex = triangle->primitiveIndex;
            candidate->triangleIndex = tri_i;
            candidate->entryTriangle = *triangle;
            candidate->boundsMin = triangle->p0;
            candidate->boundsMax = triangle->p0;
            candidate->payload = payload;
            candidate->areaScore = area;
        } else if (area > candidate->areaScore) {
            candidate->triangleIndex = tri_i;
            candidate->entryTriangle = *triangle;
            candidate->areaScore = area;
        }
        candidate->triangleCount += 1;
        runtime_caustic_transport_vec3_minmax(triangle->p0,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p1,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
        runtime_caustic_transport_vec3_minmax(triangle->p2,
                                              &candidate->boundsMin,
                                              &candidate->boundsMax);
    }

    for (int i = 0; i < MAX_OBJECTS; ++i) {
        Candidate* candidate = &candidates[i];
        RuntimeCausticTransportMeshDielectric3D resolved;
        Vec3 extent = vec3(0.0, 0.0, 0.0);
        double ex = 0.0;
        double ey = 0.0;
        double ez = 0.0;
        double min_extent = 0.0;
        double max_extent = 0.0;
        double mid_extent = 0.0;
        double score = 0.0;
        if (!candidate->seen) continue;
        extent = vec3_sub(candidate->boundsMax, candidate->boundsMin);
        ex = fmax(extent.x, 0.0);
        ey = fmax(extent.y, 0.0);
        ez = fmax(extent.z, 0.0);
        min_extent = fmin(ex, fmin(ey, ez));
        max_extent = fmax(ex, fmax(ey, ez));
        mid_extent = ex + ey + ez - min_extent - max_extent;
        if (!(max_extent > 1.0e-6)) continue;
        if (!(mid_extent > 1.0e-6)) mid_extent = max_extent;
        if (!(min_extent > 1.0e-6)) min_extent = max_extent * 0.08;

        memset(&resolved, 0, sizeof(resolved));
        resolved.valid = true;
        resolved.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.primitiveIndex = candidate->primitiveIndex;
        resolved.triangleIndex = candidate->triangleIndex;
        resolved.triangleCount = candidate->triangleCount;
        resolved.entryTriangle = candidate->entryTriangle;
        RuntimeCausticLensTransport3D_DefaultShape(&resolved.shape);
        resolved.shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC;
        resolved.shape.sceneObjectIndex = candidate->sceneObjectIndex;
        resolved.shape.primitiveIndex = candidate->primitiveIndex;
        resolved.shape.boundsMin = candidate->boundsMin;
        resolved.shape.boundsMax = candidate->boundsMax;
        resolved.shape.center = vec3_scale(vec3_add(candidate->boundsMin,
                                                    candidate->boundsMax),
                                           0.5);
        resolved.shape.axis = vec3_normalize(candidate->entryTriangle.normal);
        if (!(vec3_length(resolved.shape.axis) > 1.0e-9)) {
            resolved.shape.axis = vec3(0.0, 1.0, 0.0);
        }
        resolved.shape.radius = mid_extent * 0.5;
        resolved.shape.height = min_extent;
        resolved.shape.payload = candidate->payload;
        resolved.payload = candidate->payload;

        score = (double)candidate->triangleCount + candidate->areaScore;
        if (score > best_score) {
            best_score = score;
            best = resolved;
        }
    }

    if (!best.valid) return false;
    *out_mesh_dielectric = best;
    return true;
}
