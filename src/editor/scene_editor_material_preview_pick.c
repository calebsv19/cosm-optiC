#include "editor/scene_editor_material_preview_internal.h"

#include <float.h>
#include <math.h>
#include <string.h>

double scene_editor_material_preview_view_depth(
    const SceneEditorDigestOverlayProjector* projector,
    double world_x,
    double world_y,
    double world_z) {
    return SceneEditorDigestOverlayViewDepth(projector, world_x, world_y, world_z);
}

bool scene_editor_material_preview_barycentric_at_point(
    const SceneEditorMaterialPreviewProjectedTriangle* triangle,
    double px,
    double py,
    double* out_u,
    double* out_v,
    double* out_w) {
    double denom = 0.0;
    double bary_u = 0.0;
    double bary_v = 0.0;
    double bary_w = 0.0;
    if (!triangle || !out_u || !out_v || !out_w) return false;
    denom = ((double)(triangle->y1 - triangle->y2) * (double)(triangle->x0 - triangle->x2)) +
            ((double)(triangle->x2 - triangle->x1) * (double)(triangle->y0 - triangle->y2));
    if (fabs(denom) < 1e-9) return false;
    bary_u =
        (((double)(triangle->y1 - triangle->y2) * (px - (double)triangle->x2)) +
         ((double)(triangle->x2 - triangle->x1) * (py - (double)triangle->y2))) /
        denom;
    bary_v =
        (((double)(triangle->y2 - triangle->y0) * (px - (double)triangle->x2)) +
         ((double)(triangle->x0 - triangle->x2) * (py - (double)triangle->y2))) /
        denom;
    bary_w = 1.0 - bary_u - bary_v;
    *out_u = bary_u;
    *out_v = bary_v;
    *out_w = bary_w;
    return true;
}

bool SceneEditorMaterialPreviewPickTriangle(
    int focused_object_index,
    const SceneEditorDigestOverlayProjector* projector,
    int screen_x,
    int screen_y,
    SceneEditorMaterialPreviewTriangleAddress* out_address) {
    RuntimeScene3D scene;
    SceneEditorMaterialPreviewProjectedTriangle projected
        [SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int projected_count = 0;
    bool found = false;
    double best_depth = -DBL_MAX;
    double px = (double)screen_x + 0.5;
    double py = (double)screen_y + 0.5;
    if (out_address) memset(out_address, 0, sizeof(*out_address));
    if (!projector || focused_object_index < 0 || !out_address) return false;
    if (screen_x < projector->viewport.x ||
        screen_x >= projector->viewport.x + projector->viewport.w ||
        screen_y < projector->viewport.y ||
        screen_y >= projector->viewport.y + projector->viewport.h) {
        return false;
    }
    if (!scene_editor_material_preview_build_scene(&scene)) return false;
    memset(projected, 0, sizeof(projected));
    projected_count = scene_editor_material_preview_build_projected_triangles(
        &scene,
        focused_object_index,
        projector,
        projected,
        SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES);
    for (int i = 0; i < projected_count; ++i) {
        double bary_u = 0.0;
        double bary_v = 0.0;
        double bary_w = 0.0;
        double depth = 0.0;
        if (!scene_editor_material_preview_barycentric_at_point(&projected[i],
                                                                px,
                                                                py,
                                                                &bary_u,
                                                                &bary_v,
                                                                &bary_w)) {
            continue;
        }
        if (bary_u < -0.0001 || bary_v < -0.0001 || bary_w < -0.0001) continue;
        depth = bary_u * projected[i].depth0 +
                bary_v * projected[i].depth1 +
                bary_w * projected[i].depth2;
        if (!found || depth > best_depth) {
            best_depth = depth;
            *out_address = projected[i].address;
            found = true;
        }
    }
    RuntimeScene3D_Free(&scene);
    return found;
}
