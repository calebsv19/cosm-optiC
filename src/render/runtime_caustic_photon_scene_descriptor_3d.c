#include "render/runtime_caustic_photon_scene_descriptor_3d.h"

#include <math.h>
#include <string.h>

#include "material/material.h"
#include "render/runtime_material_payload_3d.h"

static bool photon_scene_descriptor_payload_is_mesh_dielectric(
    const RuntimeMaterialPayload3D* payload) {
    if (!payload || !payload->valid) return false;
    return payload->transparency > 1.0e-6 ||
           payload->materialId == MATERIAL_PRESET_TRANSPARENT ||
           payload->opticalIor > 1.0001 ||
           payload->bsdf.ior > 1.0001 ||
           (payload->bsdf.reflectivity > 0.10 && payload->bsdf.roughness <= 0.35);
}

static void photon_scene_descriptor_vec3_minmax(Vec3 value,
                                                Vec3* min_value,
                                                Vec3* max_value) {
    if (!min_value || !max_value) return;
    if (value.x < min_value->x) min_value->x = value.x;
    if (value.y < min_value->y) min_value->y = value.y;
    if (value.z < min_value->z) min_value->z = value.z;
    if (value.x > max_value->x) max_value->x = value.x;
    if (value.y > max_value->y) max_value->y = value.y;
    if (value.z > max_value->z) max_value->z = value.z;
}

static void photon_scene_descriptor_resolve_shape_extents(
    RuntimeCausticLensShape3D* shape,
    const RuntimeTriangle3D* entry_triangle) {
    Vec3 extent;
    double ex = 0.0;
    double ey = 0.0;
    double ez = 0.0;
    double min_extent = 0.0;
    double max_extent = 0.0;
    double mid_extent = 0.0;

    if (!shape) return;
    extent = vec3_sub(shape->boundsMax, shape->boundsMin);
    ex = fmax(extent.x, 0.0);
    ey = fmax(extent.y, 0.0);
    ez = fmax(extent.z, 0.0);
    min_extent = fmin(ex, fmin(ey, ez));
    max_extent = fmax(ex, fmax(ey, ez));
    mid_extent = ex + ey + ez - min_extent - max_extent;

    shape->center = vec3_scale(vec3_add(shape->boundsMin, shape->boundsMax), 0.5);
    shape->axis = vec3(0.0, 0.0, 1.0);
    if (ex <= ey && ex <= ez) {
        shape->axis = vec3(1.0, 0.0, 0.0);
    } else if (ey <= ex && ey <= ez) {
        shape->axis = vec3(0.0, 1.0, 0.0);
    }
    if (entry_triangle && vec3_dot(shape->axis, entry_triangle->normal) < 0.0) {
        shape->axis = vec3_scale(shape->axis, -1.0);
    }
    if (!(mid_extent > 1.0e-6)) mid_extent = max_extent;
    if (!(min_extent > 1.0e-6)) min_extent = max_extent * 0.08;
    shape->radius = mid_extent * 0.5;
    shape->height = min_extent;
}

void RuntimeCausticPhotonSceneDescriptor3D_InitBatch(
    RuntimeCausticPhotonSceneDescriptorBatch3D* batch) {
    if (!batch) return;
    memset(batch, 0, sizeof(*batch));
    batch->selectedDescriptorIndex = -1;
}

bool RuntimeCausticPhotonSceneDescriptor3D_HarvestMeshDielectricBatch(
    const RuntimeScene3D* scene,
    RuntimeCausticPhotonSceneDescriptorBatch3D* out_batch) {
    RuntimeCausticPhotonSceneDescriptorBatch3D batch;
    int best_index = -1;
    double best_score = -1.0;

    RuntimeCausticPhotonSceneDescriptor3D_InitBatch(&batch);
    batch.attempted = true;
    if (!scene) {
        if (out_batch) *out_batch = batch;
        return false;
    }

    for (int tri_i = 0; tri_i < scene->triangleMesh.triangleCount; ++tri_i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[tri_i];
        RuntimeCausticPhotonMeshDielectricDescriptor3D* descriptor = NULL;
        RuntimeMaterialPayload3D payload;
        HitInfo3D hit;
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
        hit.position = centroid;
        hit.normal = triangle->normal;
        hit.triangleIndex = tri_i;
        hit.primitiveIndex = triangle->primitiveIndex;
        hit.sceneObjectIndex = object_index;
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload) ||
            !photon_scene_descriptor_payload_is_mesh_dielectric(&payload)) {
            continue;
        }

        descriptor = &batch.descriptors[object_index];
        if (!descriptor->valid) {
            RuntimeCausticLensTransport3D_DefaultShape(&descriptor->shape);
            descriptor->valid = true;
            descriptor->sceneObjectIndex = object_index;
            descriptor->primitiveIndex = triangle->primitiveIndex;
            descriptor->triangleIndex = tri_i;
            descriptor->entryTriangle = *triangle;
            descriptor->shape.kind = RUNTIME_CAUSTIC_LENS_SHAPE_MESH_DIELECTRIC;
            descriptor->shape.sceneObjectIndex = object_index;
            descriptor->shape.primitiveIndex = triangle->primitiveIndex;
            descriptor->shape.boundsMin = triangle->p0;
            descriptor->shape.boundsMax = triangle->p0;
            descriptor->shape.payload = payload;
            descriptor->areaScore = area;
            batch.meshDielectricCandidateCount += 1u;
            batch.descriptorCount += 1;
        } else if (area > descriptor->areaScore) {
            descriptor->triangleIndex = tri_i;
            descriptor->entryTriangle = *triangle;
            descriptor->areaScore = area;
        }

        descriptor->triangleCount += 1;
        photon_scene_descriptor_vec3_minmax(triangle->p0,
                                            &descriptor->shape.boundsMin,
                                            &descriptor->shape.boundsMax);
        photon_scene_descriptor_vec3_minmax(triangle->p1,
                                            &descriptor->shape.boundsMin,
                                            &descriptor->shape.boundsMax);
        photon_scene_descriptor_vec3_minmax(triangle->p2,
                                            &descriptor->shape.boundsMin,
                                            &descriptor->shape.boundsMax);
    }

    for (int i = 0; i < MAX_OBJECTS; ++i) {
        RuntimeCausticPhotonMeshDielectricDescriptor3D* descriptor =
            &batch.descriptors[i];
        double score = 0.0;
        if (!descriptor->valid) continue;
        photon_scene_descriptor_resolve_shape_extents(&descriptor->shape,
                                                      &descriptor->entryTriangle);
        score = (double)descriptor->triangleCount + descriptor->areaScore;
        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }

    batch.selectedDescriptorIndex = best_index;
    batch.succeeded = best_index >= 0;
    if (out_batch) *out_batch = batch;
    return batch.succeeded;
}

const RuntimeCausticPhotonMeshDielectricDescriptor3D*
RuntimeCausticPhotonSceneDescriptor3D_SelectedMeshDielectric(
    const RuntimeCausticPhotonSceneDescriptorBatch3D* batch) {
    if (!batch || batch->selectedDescriptorIndex < 0 ||
        batch->selectedDescriptorIndex >= MAX_OBJECTS) {
        return NULL;
    }
    if (!batch->descriptors[batch->selectedDescriptorIndex].valid) return NULL;
    return &batch->descriptors[batch->selectedDescriptorIndex];
}

uint32_t RuntimeCausticPhotonSceneDescriptor3D_CopyMeshDielectricShapes(
    const RuntimeCausticPhotonSceneDescriptorBatch3D* batch,
    RuntimeCausticLensShape3D* out_shapes,
    uint32_t capacity) {
    uint32_t count = 0u;
    if (!batch || !out_shapes || capacity == 0u) return 0u;
    for (int descriptor_index = 0;
         descriptor_index < MAX_OBJECTS && count < capacity;
         ++descriptor_index) {
        const RuntimeCausticPhotonMeshDielectricDescriptor3D* descriptor =
            &batch->descriptors[descriptor_index];
        if (!descriptor->valid) continue;
        out_shapes[count++] = descriptor->shape;
    }
    return count;
}
