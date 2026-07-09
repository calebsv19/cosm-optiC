#include "editor/scene_editor_material_preview_internal.h"

#include <string.h>

#include "render/runtime_scene_3d_builder.h"

int scene_editor_material_preview_face_group_for_triangle(int local_triangle_index) {
    if (local_triangle_index < 0) return -1;
    return local_triangle_index / 2;
}

void scene_editor_material_preview_fill_address(
    SceneEditorMaterialPreviewTriangleAddress* out_address,
    int focused_object_index,
    int primitive_index,
    int triangle_index,
    int local_triangle_index) {
    if (!out_address) return;
    out_address->sceneObjectIndex = focused_object_index;
    out_address->primitiveIndex = primitive_index;
    out_address->triangleIndex = triangle_index;
    out_address->localTriangleIndex = local_triangle_index;
    out_address->faceGroupIndex =
        scene_editor_material_preview_face_group_for_triangle(local_triangle_index);
}

bool scene_editor_material_preview_build_scene(RuntimeScene3D* scene) {
    RuntimeSceneBridge3DPrimitiveSeedState seed_state = {0};
    if (!scene) return false;
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
    if (!seed_state.valid) return false;
    RuntimeScene3D_Init(scene);
    if (!RuntimeScene3DBuilder_BuildFromPrimitiveSeedState(scene, &seed_state)) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    return true;
}

int scene_editor_material_preview_compare_depth_desc(const void* lhs, const void* rhs) {
    const SceneEditorMaterialPreviewProjectedTriangle* a =
        (const SceneEditorMaterialPreviewProjectedTriangle*)lhs;
    const SceneEditorMaterialPreviewProjectedTriangle* b =
        (const SceneEditorMaterialPreviewProjectedTriangle*)rhs;
    if (a->depth < b->depth) return 1;
    if (a->depth > b->depth) return -1;
    return 0;
}

int scene_editor_material_preview_build_projected_triangles(
    RuntimeScene3D* scene,
    int focused_object_index,
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorMaterialPreviewProjectedTriangle* projected,
    int projected_capacity) {
    int projected_count = 0;
    if (!scene || !projector || !projected || projected_capacity <= 0 || focused_object_index < 0) {
        return 0;
    }
    for (int i = 0;
         i < scene->triangleMesh.triangleCount && projected_count < projected_capacity;
         ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        SceneEditorMaterialPreviewProjectedTriangle* out = NULL;
        if (triangle->sceneObjectIndex != focused_object_index) continue;
        out = &projected[projected_count];
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  triangle->p0.x,
                                                  triangle->p0.y,
                                                  triangle->p0.z,
                                                  &out->x0,
                                                  &out->y0) ||
            !SceneEditorDigestOverlayProjectPoint(projector,
                                                  triangle->p1.x,
                                                  triangle->p1.y,
                                                  triangle->p1.z,
                                                  &out->x1,
                                                  &out->y1) ||
            !SceneEditorDigestOverlayProjectPoint(projector,
                                                  triangle->p2.x,
                                                  triangle->p2.y,
                                                  triangle->p2.z,
                                                  &out->x2,
                                                  &out->y2)) {
            continue;
        }
        out->depth0 = scene_editor_material_preview_view_depth(projector,
                                                               triangle->p0.x,
                                                               triangle->p0.y,
                                                               triangle->p0.z);
        out->depth1 = scene_editor_material_preview_view_depth(projector,
                                                               triangle->p1.x,
                                                               triangle->p1.y,
                                                               triangle->p1.z);
        out->depth2 = scene_editor_material_preview_view_depth(projector,
                                                               triangle->p2.x,
                                                               triangle->p2.y,
                                                               triangle->p2.z);
        out->depth = (out->depth0 + out->depth1 + out->depth2) / 3.0;
        scene_editor_material_preview_fill_address(&out->address,
                                                   focused_object_index,
                                                   triangle->primitiveIndex,
                                                   i,
                                                   triangle->localTriangleIndex >= 0
                                                       ? triangle->localTriangleIndex
                                                       : projected_count);
        projected_count += 1;
    }
    return projected_count;
}

bool SceneEditorMaterialPreviewResolveFocusedTriangles(
    int focused_object_index,
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorMaterialPreviewTriangleAddress* out_addresses,
    int address_capacity,
    SceneEditorMaterialPreviewStats* out_stats) {
    RuntimeScene3D scene;
    int focused_count = 0;
    bool projected = true;
    int max_face_group = -1;
    if (out_stats) memset(out_stats, 0, sizeof(*out_stats));
    if (out_addresses && address_capacity > 0) {
        memset(out_addresses, 0, sizeof(*out_addresses) * (size_t)address_capacity);
    }
    if (focused_object_index < 0) return false;
    if (!scene_editor_material_preview_build_scene(&scene)) return false;

    for (int i = 0; i < scene.triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene.triangleMesh.triangles[i];
        SceneEditorMaterialPreviewTriangleAddress address = {0};
        int x = 0;
        int y = 0;
        if (triangle->sceneObjectIndex != focused_object_index) continue;
        scene_editor_material_preview_fill_address(&address,
                                                   focused_object_index,
                                                   triangle->primitiveIndex,
                                                   i,
                                                   triangle->localTriangleIndex >= 0
                                                       ? triangle->localTriangleIndex
                                                       : focused_count);
        if (out_addresses && focused_count < address_capacity) {
            out_addresses[focused_count] = address;
        }
        if (address.faceGroupIndex > max_face_group) {
            max_face_group = address.faceGroupIndex;
        }
        if (projector) {
            if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                      triangle->p0.x,
                                                      triangle->p0.y,
                                                      triangle->p0.z,
                                                      &x,
                                                      &y) ||
                !SceneEditorDigestOverlayProjectPoint(projector,
                                                      triangle->p1.x,
                                                      triangle->p1.y,
                                                      triangle->p1.z,
                                                      &x,
                                                      &y) ||
                !SceneEditorDigestOverlayProjectPoint(projector,
                                                      triangle->p2.x,
                                                      triangle->p2.y,
                                                      triangle->p2.z,
                                                      &x,
                                                      &y)) {
                projected = false;
            }
        } else {
            projected = false;
        }
        focused_count += 1;
    }

    if (out_stats) {
        out_stats->triangleCount = focused_count;
        out_stats->faceGroupCount = max_face_group + 1;
        out_stats->projected = projected && focused_count > 0;
    }
    RuntimeScene3D_Free(&scene);
    return focused_count > 0;
}
