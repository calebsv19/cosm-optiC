#include "render/compound_scene_runtime_apply.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

#include <stdio.h>
#include <string.h>

static void diag(char* out, size_t n, const char* text) {
    if (out && n) snprintf(out, n, "%s", text);
}

static void set_triangle(RuntimeTriangle3D* triangle, Vec3 a, Vec3 b, Vec3 c) {
    Vec3 normal = vec3_cross(vec3_sub(b, a), vec3_sub(c, a));
    if (!triangle || vec3_length(normal) <= 1e-12) return;
    triangle->p0 = a;
    triangle->p1 = b;
    triangle->p2 = c;
    triangle->normal = vec3_normalize(normal);
    triangle->hasVertexNormals = false;
}

static const RayTracingRuntimeMeshAssetInstance* find_instance(
    const RayTracingRuntimeMeshAssetSet* set, const char* object_id,
    const char* asset_id) {
    if (!set) return NULL;
    for (int i = 0; i < set->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance = &set->instances[i];
        if (!strcmp(instance->object_id, object_id) &&
            !strcmp(instance->asset_id, asset_id)) return instance;
    }
    return NULL;
}

static RuntimePrimitive3D* find_plane(RuntimeScene3D* scene, const char* object_id) {
    if (!scene) return NULL;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        RuntimePrimitive3D* primitive = &scene->primitives[i];
        if (primitive->kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE &&
            !strcmp(primitive->source.objectId, object_id)) return primitive;
    }
    return NULL;
}

static bool apply_plane(RuntimeScene3D* scene, RuntimePrimitive3D* primitive,
                        const RayCompoundSceneRoomPlane* plane) {
    Vec3 origin = vec3(plane->origin_m.x, plane->origin_m.y, plane->origin_m.z);
    Vec3 u = vec3_normalize(vec3(plane->axis_u.x, plane->axis_u.y, plane->axis_u.z));
    Vec3 v = vec3_normalize(vec3(plane->axis_v.x, plane->axis_v.y, plane->axis_v.z));
    Vec3 half_u = vec3_scale(u, plane->width_m * 0.5);
    Vec3 half_v = vec3_scale(v, plane->height_m * 0.5);
    Vec3 p0 = vec3_sub(vec3_sub(origin, half_u), half_v);
    Vec3 p1 = vec3_add(vec3_sub(origin, half_v), half_u);
    Vec3 p2 = vec3_add(vec3_add(origin, half_u), half_v);
    Vec3 p3 = vec3_add(vec3_sub(origin, half_u), half_v);
    int found = 0;
    if (!scene || !primitive || !plane) return false;
    primitive->shape.plane.origin = origin;
    primitive->shape.plane.axisU = u;
    primitive->shape.plane.axisV = v;
    primitive->shape.plane.normal = vec3_normalize(vec3_cross(u, v));
    primitive->shape.plane.width = plane->width_m;
    primitive->shape.plane.height = plane->height_m;
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        RuntimeTriangle3D* t = &scene->triangleMesh.triangles[i];
        if (t->primitiveIndex != (int)(primitive - scene->primitives)) continue;
        if (found == 0) set_triangle(t, p0, p1, p2);
        else if (found == 1) set_triangle(t, p0, p2, p3);
        ++found;
    }
    return found == 2;
}

bool ray_compound_scene_runtime_apply_exact(
    RuntimeScene3D* scene, const RayTracingRuntimeMeshAssetSet* mesh_assets,
    const RayCompoundSceneIngestionDescriptor* descriptor,
    const RayCompoundSceneIngestionResult* result, char* diagnostics,
    size_t diagnostics_size) {
    if (!scene || !mesh_assets || !descriptor || !result || !result->valid ||
        !ray_compound_scene_assembly_validate(&result->assembly)) {
        diag(diagnostics, diagnostics_size, "compound ingestion runtime apply: invalid input");
        return false;
    }
    for (size_t body = 0; body < result->assembly.simulated_count; ++body) {
        const RayCompoundSceneObjectRecord* record = &result->assembly.objects[body];
        const RayCompoundSceneDetachedGeometry* geometry =
            &result->assembly.simulated_geometry[body];
        const RayTracingRuntimeMeshAssetInstance* instance =
            find_instance(mesh_assets, record->object_id, record->geometry_id);
        const CoreMeshAssetRuntimeDocument* document = NULL;
        int changed = 0;
        if (!instance || instance->asset_index < 0 ||
            instance->asset_index >= mesh_assets->asset_count) {
            diag(diagnostics, diagnostics_size, "compound ingestion runtime apply: bound mesh missing");
            return false;
        }
        document = &mesh_assets->assets[instance->asset_index].document;
        if (geometry->vertex_count != document->vertex_count) {
            diag(diagnostics, diagnostics_size, "compound ingestion runtime apply: mesh vertex count mismatch");
            return false;
        }
        for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
            RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
            const CoreMeshAssetRuntimeTriangle* source = NULL;
            if (triangle->primitiveIndex < 0 || triangle->localTriangleIndex < 0 ||
                triangle->localTriangleIndex >= (int)document->triangle_count) continue;
            if (triangle->sceneObjectIndex != instance->scene_object_index) continue;
            source = &document->triangles[triangle->localTriangleIndex];
            set_triangle(triangle,
                         vec3(geometry->world_positions[source->a].x,
                              geometry->world_positions[source->a].y,
                              geometry->world_positions[source->a].z),
                         vec3(geometry->world_positions[source->b].x,
                              geometry->world_positions[source->b].y,
                              geometry->world_positions[source->b].z),
                         vec3(geometry->world_positions[source->c].x,
                              geometry->world_positions[source->c].y,
                              geometry->world_positions[source->c].z));
            ++changed;
        }
        if (changed != (int)document->triangle_count) {
            diag(diagnostics, diagnostics_size, "compound ingestion runtime apply: bound mesh topology mismatch");
            return false;
        }
    }
    for (size_t role = 0; role < result->room.plane_count; ++role) {
        if (!result->room.planes[role].render_visible) continue;
        RuntimePrimitive3D* plane = find_plane(scene, descriptor->room_object_ids[role]);
        if (!plane || !apply_plane(scene, plane, &result->room.planes[role])) {
            diag(diagnostics, diagnostics_size, "compound ingestion runtime apply: bound room plane missing");
            return false;
        }
    }
    scene->triangleMesh.bvhDirty = true;
    /* Mesh-asset records built the source scene.  Reapplying them here would
       overwrite the just-resolved exact world-space triangles. */
    if (!RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene) ||
        !RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh)) {
        diag(diagnostics, diagnostics_size,
             "compound ingestion runtime apply: acceleration rebuild failed");
        return false;
    }
    diag(diagnostics, diagnostics_size, "ok");
    return true;
}
