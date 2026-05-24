#include "editor/scene_editor_material_preview.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/scene_editor_material_face_metrics.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_material_texture_stack_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "scene/object_manager.h"

typedef struct SceneEditorMaterialPreviewProjectedTriangle {
    int x0;
    int y0;
    int x1;
    int y1;
    int x2;
    int y2;
    double depth;
    double depth0;
    double depth1;
    double depth2;
    SceneEditorMaterialPreviewTriangleAddress address;
} SceneEditorMaterialPreviewProjectedTriangle;

#define SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TEXTURE_BLOCKS 4096
#define SCENE_EDITOR_MATERIAL_PREVIEW_MAX_SAMPLE_STEP 16

static SDL_Color scene_editor_material_preview_color_from_packed_rgb(int packed, Uint8 alpha) {
    SDL_Color out = {220, 200, 88, alpha};
    const double max_preview_channel = 168.0;
    double max_channel = 0.0;
    double scale = 1.0;
    if (packed != 0) {
        out.r = (Uint8)((packed >> 16) & 0xFF);
        out.g = (Uint8)((packed >> 8) & 0xFF);
        out.b = (Uint8)(packed & 0xFF);
    }
    max_channel = fmax((double)out.r, fmax((double)out.g, (double)out.b));
    if (max_channel > max_preview_channel) {
        scale = max_preview_channel / max_channel;
        out.r = (Uint8)lround((double)out.r * scale);
        out.g = (Uint8)lround((double)out.g * scale);
        out.b = (Uint8)lround((double)out.b * scale);
    }
    return out;
}

static SDL_Color scene_editor_material_preview_dim_base_color(Uint8 alpha) {
    SDL_Color out = {44, 46, 52, alpha};
    return out;
}

static SDL_Color scene_editor_material_preview_edge_color(SDL_Color fill) {
    SDL_Color out = {
        (Uint8)fmax(0.0, (double)fill.r * 0.42),
        (Uint8)fmax(0.0, (double)fill.g * 0.42),
        (Uint8)fmax(0.0, (double)fill.b * 0.42),
        255
    };
    return out;
}

static double scene_editor_material_preview_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool scene_editor_material_preview_color_from_surface_eval(
    const RuntimeMaterialSurfaceEval* surface_eval,
    SDL_Color base_color,
    SDL_Color* out_color,
    double* out_mask) {
    SDL_Color color = base_color;
    double mask = 0.0;
    if (!out_color) return false;
    if (surface_eval && surface_eval->active) {
        mask = scene_editor_material_preview_clamp01(surface_eval->textureMask);
        color.r = (Uint8)lround(scene_editor_material_preview_clamp01(surface_eval->colorR) * 255.0);
        color.g = (Uint8)lround(scene_editor_material_preview_clamp01(surface_eval->colorG) * 255.0);
        color.b = (Uint8)lround(scene_editor_material_preview_clamp01(surface_eval->colorB) * 255.0);
    }
    color.a = base_color.a;
    *out_color = color;
    if (out_mask) *out_mask = mask;
    return mask > 1e-9;
}

static bool scene_editor_material_preview_apply_face_override_to_stack(
    const SceneObject* object,
    int scene_object_index,
    const SceneEditorMaterialFacePlacement* placement,
    RuntimeMaterialTextureStack* stack) {
    RuntimeMaterialTextureLayer layer = {0};
    if (!object || !placement || !stack) return false;
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, stack)) {
        return false;
    }
    if (placement->layerIndex < 0 || placement->layerIndex >= stack->layerCount) {
        return false;
    }
    layer = stack->layers[placement->layerIndex];
    layer.placement.textureId = placement->textureId;
    layer.placement.offsetU = placement->offsetU;
    layer.placement.offsetV = placement->offsetV;
    layer.placement.scale = placement->scale;
    layer.placement.strength = placement->strength;
    layer.placement.rotation = placement->rotation;
    layer.params = placement->params;
    layer.placement.params = placement->params;
    stack->layers[placement->layerIndex] = RuntimeMaterialTextureLayerNormalize(layer);
    *stack = RuntimeMaterialTextureStackNormalize(*stack);
    return true;
}

bool SceneEditorMaterialPreviewEvaluateTextureColor(const SceneObject* object,
                                                    int triangle_index,
                                                    double bary_v,
                                                    double bary_w,
                                                    SDL_Color base_color,
                                                    SDL_Color* out_color,
                                                    double* out_mask) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase((double)base_color.r / 255.0,
                                           (double)base_color.g / 255.0,
                                           (double)base_color.b / 255.0,
                                           0.5,
                                           0.0,
                                           0.0,
                                           1.0,
                                           0.0);
    RuntimeMaterialSurfaceEval surface_eval = {0};
    RuntimeMaterialTextureStackBuildLegacyFromObject(object, &stack);
    RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                object,
                                                bary_v,
                                                bary_w,
                                                triangle_index + 1,
                                                &base_eval,
                                                &surface_eval);
    return scene_editor_material_preview_color_from_surface_eval(&surface_eval,
                                                                 base_color,
                                                                 out_color,
                                                                 out_mask);
}

bool SceneEditorMaterialPreviewEvaluateTextureColorForFace(
    const SceneObject* object,
    int scene_object_index,
    int primitive_index,
    int face_group_index,
    int local_triangle_index,
    int triangle_index,
    double bary_u,
    double bary_v,
    double bary_w,
                                                    SDL_Color base_color,
                                                    SDL_Color* out_color,
                                                    double* out_mask) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase((double)base_color.r / 255.0,
                                           (double)base_color.g / 255.0,
                                           (double)base_color.b / 255.0,
                                           0.5,
                                           0.0,
                                           0.0,
                                           1.0,
                                           0.0);
    RuntimeMaterialSurfaceEval surface_eval = {0};
    SceneEditorMaterialFacePlacement placement = {0};
    RuntimeMaterialTexture3DPlacement runtime_placement = {0};
    bool has_face_override =
        SceneEditorMaterialFacePlacementHasOverride(scene_object_index, face_group_index);
    double island_u = 0.0;
    double island_v = 0.0;
    double grounded_u = 0.0;
    double grounded_v = 0.0;
    int seed_key = ((scene_object_index + 1) * 19349663) ^ ((face_group_index + 1) * 83492791);
    if (seed_key == 0) seed_key = triangle_index + 1;
    SceneEditorMaterialFacePlacementResolveIslandUV(local_triangle_index,
                                                    bary_u,
                                                    bary_v,
                                                    bary_w,
                                                    &island_u,
                                                    &island_v);
    SceneEditorMaterialFaceMetricsResolveGroundedUV(primitive_index,
                                                    scene_object_index,
                                                    face_group_index,
                                                    island_u,
                                                    island_v,
                                                    &grounded_u,
                                                    &grounded_v);
    if (has_face_override) {
        placement = SceneEditorMaterialFacePlacementGetEffective(object,
                                                                scene_object_index,
                                                                face_group_index);
        if (!scene_editor_material_preview_apply_face_override_to_stack(object,
                                                                        scene_object_index,
                                                                        &placement,
                                                                        &stack)) {
            runtime_placement = SceneEditorMaterialFacePlacementToRuntime(&placement);
            RuntimeMaterialTextureStackBuildLegacyFromPlacement(&runtime_placement, &stack);
        }
    } else {
        SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, &stack);
    }
    RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                object,
                                                grounded_u,
                                                grounded_v,
                                                seed_key,
                                                &base_eval,
                                                &surface_eval);
    return scene_editor_material_preview_color_from_surface_eval(&surface_eval,
                                                                 base_color,
                                                                 out_color,
                                                                 out_mask);
}

static double scene_editor_material_preview_view_depth(
    const SceneEditorDigestOverlayProjector* projector,
    double world_x,
    double world_y,
    double world_z) {
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    double yaw_y = 0.0;
    double yaw_z = 0.0;
    if (!projector) return 0.0;
    px = world_x - projector->center_x;
    py = world_y - projector->center_y;
    pz = world_z - projector->center_z;
    yaw_y = sin(projector->yaw_rad) * px + cos(projector->yaw_rad) * py;
    yaw_z = pz;
    return sin(projector->pitch_rad) * yaw_y + cos(projector->pitch_rad) * yaw_z;
}

static int scene_editor_material_preview_face_group_for_triangle(int local_triangle_index) {
    if (local_triangle_index < 0) return -1;
    return local_triangle_index / 2;
}

static void scene_editor_material_preview_fill_address(
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

static bool scene_editor_material_preview_build_scene(RuntimeScene3D* scene) {
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
                                                   focused_count);
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

static int scene_editor_material_preview_compare_depth_desc(const void* lhs, const void* rhs) {
    const SceneEditorMaterialPreviewProjectedTriangle* a =
        (const SceneEditorMaterialPreviewProjectedTriangle*)lhs;
    const SceneEditorMaterialPreviewProjectedTriangle* b =
        (const SceneEditorMaterialPreviewProjectedTriangle*)rhs;
    if (a->depth < b->depth) return 1;
    if (a->depth > b->depth) return -1;
    return 0;
}

static int scene_editor_material_preview_build_projected_triangles(
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
         i < scene->triangleMesh.triangleCount &&
         projected_count < projected_capacity;
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
                                                   projected_count);
        projected_count += 1;
    }
    return projected_count;
}

static bool scene_editor_material_preview_barycentric_at_point(
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
    projected_count = scene_editor_material_preview_build_projected_triangles(&scene,
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

static bool scene_editor_material_preview_stack_has_visual_layers(
    const RuntimeMaterialTextureStack* stack) {
    RuntimeMaterialTextureStack normalized;
    int limit = 0;
    if (!stack) return false;
    normalized = RuntimeMaterialTextureStackNormalize(*stack);
    limit = normalized.layerCount;
    if (limit > RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        limit = RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS;
    }
    for (int i = 0; i < limit; ++i) {
        const RuntimeMaterialTextureLayer* layer = &normalized.layers[i];
        if (!layer->enabled ||
            layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE ||
            layer->opacity <= 1e-9 ||
            layer->placement.strength <= 1e-9) {
            continue;
        }
        if (layer->role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE) {
            if (layer->kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID) {
                return true;
            }
            continue;
        }
        if (layer->role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_OVERLAY) {
            return true;
        }
    }
    return false;
}

static bool scene_editor_material_preview_object_has_active_texture(const SceneObject* object,
                                                                    int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (!object) return false;
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, &stack)) {
        return false;
    }
    return scene_editor_material_preview_stack_has_visual_layers(&stack);
}

static bool scene_editor_material_preview_object_base_layer_disabled(
    const SceneObject* object,
    int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (!object) return false;
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, &stack)) {
        return false;
    }
    stack = RuntimeMaterialTextureStackNormalize(stack);
    if (stack.layerCount <= 0) return false;
    if (!RuntimeMaterialTextureLayerKindIsBase(stack.layers[0].kind)) return false;
    return !stack.layers[0].enabled;
}

static bool scene_editor_material_preview_has_active_texture_for_object(const SceneObject* object,
                                                                        int scene_object_index) {
    return scene_editor_material_preview_object_has_active_texture(object, scene_object_index) ||
           SceneEditorMaterialFacePlacementObjectHasActiveTextureOverride(scene_object_index);
}

static void scene_editor_material_preview_sort_ints(int* a, int* b, int* c) {
    int tmp = 0;
    if (!a || !b || !c) return;
    if (*a > *b) {
        tmp = *a;
        *a = *b;
        *b = tmp;
    }
    if (*a > *c) {
        tmp = *a;
        *a = *c;
        *c = tmp;
    }
    if (*b > *c) {
        tmp = *b;
        *b = *c;
        *c = tmp;
    }
}

static double scene_editor_material_preview_edge_intersection_x(int y,
                                                                int x0,
                                                                int y0,
                                                                int x1,
                                                                int y1) {
    double t = 0.0;
    if (y0 == y1) return (double)x0;
    t = ((double)y - (double)y0) / ((double)y1 - (double)y0);
    return (double)x0 + ((double)x1 - (double)x0) * t;
}

static void scene_editor_material_preview_draw_flat_bottom_triangle(SDL_Renderer* renderer,
                                                                    int x0,
                                                                    int y0,
                                                                    int x1,
                                                                    int y1,
                                                                    int x2,
                                                                    int y2) {
    int y = 0;
    if (!renderer || y1 == y0 || y2 == y0) return;
    for (y = y0; y <= y1; ++y) {
        double ax = scene_editor_material_preview_edge_intersection_x(y, x0, y0, x1, y1);
        double bx = scene_editor_material_preview_edge_intersection_x(y, x0, y0, x2, y2);
        if (ax > bx) {
            double tmp = ax;
            ax = bx;
            bx = tmp;
        }
        SDL_RenderDrawLine(renderer, (int)lround(ax), y, (int)lround(bx), y);
    }
}

static void scene_editor_material_preview_draw_flat_top_triangle(SDL_Renderer* renderer,
                                                                 int x0,
                                                                 int y0,
                                                                 int x1,
                                                                 int y1,
                                                                 int x2,
                                                                 int y2) {
    int y = 0;
    if (!renderer || y2 == y0 || y2 == y1) return;
    for (y = y0; y <= y2; ++y) {
        double ax = scene_editor_material_preview_edge_intersection_x(y, x0, y0, x2, y2);
        double bx = scene_editor_material_preview_edge_intersection_x(y, x1, y1, x2, y2);
        if (ax > bx) {
            double tmp = ax;
            ax = bx;
            bx = tmp;
        }
        SDL_RenderDrawLine(renderer, (int)lround(ax), y, (int)lround(bx), y);
    }
}

static void scene_editor_material_preview_fill_solid_triangle(
    SDL_Renderer* renderer,
    const SceneEditorMaterialPreviewProjectedTriangle* triangle,
    SDL_Color color) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
    if (!renderer || !triangle) return;
    x0 = triangle->x0;
    y0 = triangle->y0;
    x1 = triangle->x1;
    y1 = triangle->y1;
    x2 = triangle->x2;
    y2 = triangle->y2;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (y0 == y1 && y1 == y2) {
        scene_editor_material_preview_sort_ints(&x0, &x1, &x2);
        SDL_RenderDrawLine(renderer, x0, y0, x2, y0);
        return;
    }
    if (y0 > y1) {
        int tx = x0;
        int ty = y0;
        x0 = x1;
        y0 = y1;
        x1 = tx;
        y1 = ty;
    }
    if (y0 > y2) {
        int tx = x0;
        int ty = y0;
        x0 = x2;
        y0 = y2;
        x2 = tx;
        y2 = ty;
    }
    if (y1 > y2) {
        int tx = x1;
        int ty = y1;
        x1 = x2;
        y1 = y2;
        x2 = tx;
        y2 = ty;
    }
    if (y1 == y2) {
        scene_editor_material_preview_draw_flat_bottom_triangle(renderer, x0, y0, x1, y1, x2, y2);
    } else if (y0 == y1) {
        scene_editor_material_preview_draw_flat_top_triangle(renderer, x0, y0, x1, y1, x2, y2);
    } else {
        double split_t = ((double)y1 - (double)y0) / ((double)y2 - (double)y0);
        int split_x = (int)lround((double)x0 + ((double)x2 - (double)x0) * split_t);
        scene_editor_material_preview_draw_flat_bottom_triangle(renderer,
                                                                x0,
                                                                y0,
                                                                x1,
                                                                y1,
                                                                split_x,
                                                                y1);
        scene_editor_material_preview_draw_flat_top_triangle(renderer,
                                                             x1,
                                                             y1,
                                                             split_x,
                                                             y1,
                                                             x2,
                                                             y2);
    }
}

static void scene_editor_material_preview_fill_sampled_triangle(
    SDL_Renderer* renderer,
    const SceneEditorMaterialPreviewProjectedTriangle* triangle,
    const SceneObject* object,
    const SDL_Rect* viewport,
    SDL_Color base_color,
    double* z_buffer,
    int z_buffer_width,
    bool sample_texture) {
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    double denom = 0.0;
    int area = 0;
    int step = 1;
    if (!renderer || !triangle || !viewport) return;
    if (sample_texture && !object) return;
    min_x = triangle->x0;
    if (triangle->x1 < min_x) min_x = triangle->x1;
    if (triangle->x2 < min_x) min_x = triangle->x2;
    max_x = triangle->x0;
    if (triangle->x1 > max_x) max_x = triangle->x1;
    if (triangle->x2 > max_x) max_x = triangle->x2;
    min_y = triangle->y0;
    if (triangle->y1 < min_y) min_y = triangle->y1;
    if (triangle->y2 < min_y) min_y = triangle->y2;
    max_y = triangle->y0;
    if (triangle->y1 > max_y) max_y = triangle->y1;
    if (triangle->y2 > max_y) max_y = triangle->y2;
    if (min_x < viewport->x) min_x = viewport->x;
    if (min_y < viewport->y) min_y = viewport->y;
    if (max_x >= viewport->x + viewport->w) max_x = viewport->x + viewport->w - 1;
    if (max_y >= viewport->y + viewport->h) max_y = viewport->y + viewport->h - 1;
    if (max_x < min_x || max_y < min_y) return;
    area = (max_x - min_x + 1) * (max_y - min_y + 1);
    if (area > SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TEXTURE_BLOCKS) {
        step = (int)ceil(sqrt((double)area /
                              (double)SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TEXTURE_BLOCKS));
        if (step < 1) step = 1;
        if (step > SCENE_EDITOR_MATERIAL_PREVIEW_MAX_SAMPLE_STEP) {
            step = SCENE_EDITOR_MATERIAL_PREVIEW_MAX_SAMPLE_STEP;
        }
    }
    denom = ((double)(triangle->y1 - triangle->y2) * (double)(triangle->x0 - triangle->x2)) +
            ((double)(triangle->x2 - triangle->x1) * (double)(triangle->y0 - triangle->y2));
    if (fabs(denom) < 1e-9) return;
    for (int y = min_y; y <= max_y; y += step) {
        for (int x = min_x; x <= max_x; x += step) {
            double px = (double)x + 0.5;
            double py = (double)y + 0.5;
            double bary_u =
                (((double)(triangle->y1 - triangle->y2) * (px - (double)triangle->x2)) +
                 ((double)(triangle->x2 - triangle->x1) * (py - (double)triangle->y2))) /
                denom;
            double bary_v =
                (((double)(triangle->y2 - triangle->y0) * (px - (double)triangle->x2)) +
                 ((double)(triangle->x0 - triangle->x2) * (py - (double)triangle->y2))) /
                denom;
            double bary_w = 1.0 - bary_u - bary_v;
            SDL_Color sample_color = base_color;
            double depth = 0.0;
            int z_index = 0;
            if (bary_u < -0.0001 || bary_v < -0.0001 || bary_w < -0.0001) continue;
            depth = bary_u * triangle->depth0 +
                    bary_v * triangle->depth1 +
                    bary_w * triangle->depth2;
            if (z_buffer && z_buffer_width > 0) {
                z_index = (y - viewport->y) * z_buffer_width + (x - viewport->x);
                if (depth <= z_buffer[z_index]) continue;
            }
            if (sample_texture) {
                SceneEditorMaterialPreviewEvaluateTextureColorForFace(
                    object,
                    triangle->address.sceneObjectIndex,
                    triangle->address.primitiveIndex,
                    triangle->address.faceGroupIndex,
                    triangle->address.localTriangleIndex,
                    triangle->address.triangleIndex,
                    bary_u,
                    bary_v,
                    bary_w,
                    base_color,
                    &sample_color,
                    NULL);
            }
            SDL_SetRenderDrawColor(renderer,
                                   sample_color.r,
                                   sample_color.g,
                                   sample_color.b,
                                   sample_color.a);
            if (step == 1) {
                SDL_RenderDrawPoint(renderer, x, y);
                if (z_buffer && z_buffer_width > 0) z_buffer[z_index] = depth;
            } else {
                SDL_Rect block = {
                    x,
                    y,
                    (x + step - 1 <= max_x) ? step : (max_x - x + 1),
                    (y + step - 1 <= max_y) ? step : (max_y - y + 1)
                };
                SDL_RenderFillRect(renderer, &block);
                if (z_buffer && z_buffer_width > 0) {
                    for (int by = block.y; by < block.y + block.h; ++by) {
                        for (int bx = block.x; bx < block.x + block.w; ++bx) {
                            int block_index =
                                (by - viewport->y) * z_buffer_width + (bx - viewport->x);
                            z_buffer[block_index] = depth;
                        }
                    }
                }
            }
        }
    }
}

static void scene_editor_material_preview_draw_edges(
    SDL_Renderer* renderer,
    const SceneEditorMaterialPreviewProjectedTriangle* triangle) {
    if (!renderer || !triangle) return;
    SDL_RenderDrawLine(renderer, triangle->x0, triangle->y0, triangle->x1, triangle->y1);
    SDL_RenderDrawLine(renderer, triangle->x1, triangle->y1, triangle->x2, triangle->y2);
    SDL_RenderDrawLine(renderer, triangle->x2, triangle->y2, triangle->x0, triangle->y0);
}

static void scene_editor_material_preview_edge_points(
    const SceneEditorMaterialPreviewProjectedTriangle* triangle,
    int edge_index,
    int* out_x0,
    int* out_y0,
    double* out_depth0,
    int* out_x1,
    int* out_y1,
    double* out_depth1) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    double depth0 = 0.0;
    double depth1 = 0.0;
    if (!triangle || !out_x0 || !out_y0 || !out_depth0 || !out_x1 || !out_y1 || !out_depth1) {
        return;
    }
    if (edge_index == 0) {
        x0 = triangle->x0;
        y0 = triangle->y0;
        depth0 = triangle->depth0;
        x1 = triangle->x1;
        y1 = triangle->y1;
        depth1 = triangle->depth1;
    } else if (edge_index == 1) {
        x0 = triangle->x1;
        y0 = triangle->y1;
        depth0 = triangle->depth1;
        x1 = triangle->x2;
        y1 = triangle->y2;
        depth1 = triangle->depth2;
    } else {
        x0 = triangle->x2;
        y0 = triangle->y2;
        depth0 = triangle->depth2;
        x1 = triangle->x0;
        y1 = triangle->y0;
        depth1 = triangle->depth0;
    }
    *out_x0 = x0;
    *out_y0 = y0;
    *out_depth0 = depth0;
    *out_x1 = x1;
    *out_y1 = y1;
    *out_depth1 = depth1;
}

static void scene_editor_material_preview_draw_visible_edge(
    SDL_Renderer* renderer,
    const SDL_Rect* viewport,
    const double* z_buffer,
    int z_buffer_width,
    int x0,
    int y0,
    double depth0,
    int x1,
    int y1,
    double depth1,
    double depth_epsilon) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = abs(dx);
    if (abs(dy) > steps) steps = abs(dy);
    if (!renderer || !viewport || !z_buffer || z_buffer_width <= 0) return;
    if (steps <= 0) {
        if (x0 >= viewport->x && x0 < viewport->x + viewport->w &&
            y0 >= viewport->y && y0 < viewport->y + viewport->h) {
            int z_index = (y0 - viewport->y) * z_buffer_width + (x0 - viewport->x);
            if (depth0 >= z_buffer[z_index] - depth_epsilon) {
                SDL_RenderDrawPoint(renderer, x0, y0);
            }
        }
        return;
    }
    for (int i = 0; i <= steps; ++i) {
        double t = (double)i / (double)steps;
        int x = (int)lround((double)x0 + (double)dx * t);
        int y = (int)lround((double)y0 + (double)dy * t);
        double depth = depth0 + (depth1 - depth0) * t;
        int z_index = 0;
        if (x < viewport->x || x >= viewport->x + viewport->w ||
            y < viewport->y || y >= viewport->y + viewport->h) {
            continue;
        }
        z_index = (y - viewport->y) * z_buffer_width + (x - viewport->x);
        if (depth >= z_buffer[z_index] - depth_epsilon) {
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
}

static void scene_editor_material_preview_draw_solid_visible_edges(
    SDL_Renderer* renderer,
    const SceneEditorMaterialPreviewProjectedTriangle* triangles,
    int triangle_count,
    const SDL_Rect* viewport,
    const double* z_buffer,
    int z_buffer_width,
    double depth_epsilon) {
    if (!renderer || !triangles || triangle_count <= 0 || !viewport || !z_buffer) return;
    for (int i = 0; i < triangle_count; ++i) {
        for (int edge = 0; edge < 3; ++edge) {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            double depth0 = 0.0;
            double depth1 = 0.0;
            scene_editor_material_preview_edge_points(&triangles[i],
                                                      edge,
                                                      &x0,
                                                      &y0,
                                                      &depth0,
                                                      &x1,
                                                      &y1,
                                                      &depth1);
            scene_editor_material_preview_draw_visible_edge(renderer,
                                                            viewport,
                                                            z_buffer,
                                                            z_buffer_width,
                                                            x0,
                                                            y0,
                                                            depth0,
                                                            x1,
                                                            y1,
                                                            depth1,
                                                            depth_epsilon);
        }
    }
}

static bool scene_editor_material_preview_address_matches(
    const SceneEditorMaterialPreviewTriangleAddress* lhs,
    const SceneEditorMaterialPreviewTriangleAddress* rhs) {
    if (!lhs || !rhs) return false;
    return lhs->sceneObjectIndex == rhs->sceneObjectIndex &&
           lhs->primitiveIndex == rhs->primitiveIndex &&
           lhs->triangleIndex == rhs->triangleIndex &&
           lhs->localTriangleIndex == rhs->localTriangleIndex &&
           lhs->faceGroupIndex == rhs->faceGroupIndex;
}

static bool scene_editor_material_preview_triangle_selected(
    const SceneEditorMaterialPreviewProjectedTriangle* triangle,
    const SceneEditorMaterialPreviewTriangleAddress* selected_triangles,
    int selected_triangle_count) {
    if (!triangle || !selected_triangles || selected_triangle_count <= 0) return false;
    for (int i = 0; i < selected_triangle_count; ++i) {
        if (scene_editor_material_preview_address_matches(&triangle->address,
                                                          &selected_triangles[i])) {
            return true;
        }
    }
    return false;
}

static bool scene_editor_material_preview_edge_points_match(int ax0,
                                                            int ay0,
                                                            int ax1,
                                                            int ay1,
                                                            int bx0,
                                                            int by0,
                                                            int bx1,
                                                            int by1) {
    return (ax0 == bx0 && ay0 == by0 && ax1 == bx1 && ay1 == by1) ||
           (ax0 == bx1 && ay0 == by1 && ax1 == bx0 && ay1 == by0);
}

static bool scene_editor_material_preview_selected_edge_is_face_internal(
    const SceneEditorMaterialPreviewProjectedTriangle* triangles,
    int triangle_count,
    int current_index,
    int edge_index,
    const SceneEditorMaterialPreviewTriangleAddress* selected_triangles,
    int selected_triangle_count) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    double depth0 = 0.0;
    double depth1 = 0.0;
    if (!triangles || current_index < 0 || current_index >= triangle_count) return false;
    scene_editor_material_preview_edge_points(&triangles[current_index],
                                              edge_index,
                                              &x0,
                                              &y0,
                                              &depth0,
                                              &x1,
                                              &y1,
                                              &depth1);
    for (int i = 0; i < triangle_count; ++i) {
        if (i == current_index) continue;
        if (triangles[i].address.faceGroupIndex !=
            triangles[current_index].address.faceGroupIndex) {
            continue;
        }
        if (!scene_editor_material_preview_triangle_selected(&triangles[i],
                                                             selected_triangles,
                                                             selected_triangle_count)) {
            continue;
        }
        for (int other_edge = 0; other_edge < 3; ++other_edge) {
            int ox0 = 0;
            int oy0 = 0;
            int ox1 = 0;
            int oy1 = 0;
            double odepth0 = 0.0;
            double odepth1 = 0.0;
            scene_editor_material_preview_edge_points(&triangles[i],
                                                      other_edge,
                                                      &ox0,
                                                      &oy0,
                                                      &odepth0,
                                                      &ox1,
                                                      &oy1,
                                                      &odepth1);
            if (scene_editor_material_preview_edge_points_match(x0,
                                                                y0,
                                                                x1,
                                                                y1,
                                                                ox0,
                                                                oy0,
                                                                ox1,
                                                                oy1)) {
                return true;
            }
        }
    }
    (void)depth0;
    (void)depth1;
    return false;
}

static void scene_editor_material_preview_draw_selected_triangle_edges(
    SDL_Renderer* renderer,
    const SceneEditorMaterialPreviewProjectedTriangle* triangles,
    int triangle_count,
    int triangle_index,
    const SDL_Rect* viewport,
    const double* z_buffer,
    int z_buffer_width,
    const SceneEditorMaterialPreviewTriangleAddress* selected_triangles,
    int selected_triangle_count,
    SDL_Color color,
    double depth_epsilon) {
    const SceneEditorMaterialPreviewProjectedTriangle* triangle = NULL;
    if (!renderer || !triangles || triangle_index < 0 || triangle_index >= triangle_count) return;
    triangle = &triangles[triangle_index];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int edge = 0; edge < 3; ++edge) {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        double depth0 = 0.0;
        double depth1 = 0.0;
        if (scene_editor_material_preview_selected_edge_is_face_internal(triangles,
                                                                         triangle_count,
                                                                         triangle_index,
                                                                         edge,
                                                                         selected_triangles,
                                                                         selected_triangle_count)) {
            continue;
        }
        scene_editor_material_preview_edge_points(triangle,
                                                  edge,
                                                  &x0,
                                                  &y0,
                                                  &depth0,
                                                  &x1,
                                                  &y1,
                                                  &depth1);
        if (z_buffer && viewport && z_buffer_width > 0) {
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    scene_editor_material_preview_draw_visible_edge(renderer,
                                                                    viewport,
                                                                    z_buffer,
                                                                    z_buffer_width,
                                                                    x0 + ox,
                                                                    y0 + oy,
                                                                    depth0,
                                                                    x1 + ox,
                                                                    y1 + oy,
                                                                    depth1,
                                                                    depth_epsilon);
                }
            }
        } else {
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    SDL_RenderDrawLine(renderer, x0 + ox, y0 + oy, x1 + ox, y1 + oy);
                }
            }
        }
    }
}

static void scene_editor_material_preview_draw_selection_overlay(
    SDL_Renderer* renderer,
    const SceneEditorMaterialPreviewProjectedTriangle* triangles,
    int triangle_count,
    const SDL_Rect* viewport,
    const double* z_buffer,
    int z_buffer_width,
    const SceneEditorMaterialPreviewTriangleAddress* selected_triangles,
    int selected_triangle_count,
    double depth_epsilon) {
    SDL_Color edge_color = {255, 248, 160, 255};
    if (!renderer || !triangles || triangle_count <= 0 ||
        !selected_triangles || selected_triangle_count <= 0) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, edge_color.r, edge_color.g, edge_color.b, edge_color.a);
    for (int i = 0; i < triangle_count; ++i) {
        if (!scene_editor_material_preview_triangle_selected(&triangles[i],
                                                             selected_triangles,
                                                             selected_triangle_count)) {
            continue;
        }
        scene_editor_material_preview_draw_selected_triangle_edges(renderer,
                                                                  triangles,
                                                                  triangle_count,
                                                                  i,
                                                                  viewport,
                                                                  z_buffer,
                                                                  z_buffer_width,
                                                                  selected_triangles,
                                                                  selected_triangle_count,
                                                                  edge_color,
                                                                  depth_epsilon);
    }
}

bool SceneEditorMaterialPreviewRenderFocusedObjectWithSelection(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int focused_object_index,
    bool solid_faces,
    const SceneEditorMaterialPreviewTriangleAddress* selected_triangles,
    int selected_triangle_count) {
    RuntimeScene3D scene;
    SceneEditorMaterialPreviewProjectedTriangle projected
        [SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int projected_count = 0;
    SDL_Color fill_color = {0, 0, 0, 0};
    SDL_Color edge_color = {0, 0, 0, 0};
    double* z_buffer = NULL;
    int z_buffer_count = 0;
    if (!renderer || !projector || focused_object_index < 0) return false;
    if (!scene_editor_material_preview_build_scene(&scene)) return false;
    memset(projected, 0, sizeof(projected));

    projected_count = scene_editor_material_preview_build_projected_triangles(
        &scene,
        focused_object_index,
        projector,
        projected,
        SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES);

    if (projected_count <= 0) {
        RuntimeScene3D_Free(&scene);
        return false;
    }

    qsort(projected,
          (size_t)projected_count,
          sizeof(projected[0]),
          scene_editor_material_preview_compare_depth_desc);
    if (solid_faces && projector->viewport.w > 0 && projector->viewport.h > 0) {
        z_buffer_count = projector->viewport.w * projector->viewport.h;
        z_buffer = (double*)malloc(sizeof(double) * (size_t)z_buffer_count);
        if (z_buffer) {
            for (int i = 0; i < z_buffer_count; ++i) {
                z_buffer[i] = -DBL_MAX;
            }
        }
    }
    if (solid_faces && !z_buffer) {
        solid_faces = false;
    }
    if (focused_object_index >= 0 && focused_object_index < sceneSettings.objectCount) {
        fill_color = scene_editor_material_preview_color_from_packed_rgb(
            sceneSettings.sceneObjects[focused_object_index].color,
            solid_faces ? 255 : 218);
        if (scene_editor_material_preview_object_base_layer_disabled(
                &sceneSettings.sceneObjects[focused_object_index],
                focused_object_index)) {
            fill_color = scene_editor_material_preview_dim_base_color(solid_faces ? 255 : 218);
        }
    } else {
        fill_color = scene_editor_material_preview_color_from_packed_rgb(0, solid_faces ? 255 : 218);
    }
    edge_color = scene_editor_material_preview_edge_color(fill_color);

    const SceneObject* focused_object = NULL;
    if (focused_object_index >= 0 && focused_object_index < sceneSettings.objectCount) {
        focused_object = &sceneSettings.sceneObjects[focused_object_index];
    }
    for (int i = 0; i < projected_count; ++i) {
        bool has_texture =
            scene_editor_material_preview_has_active_texture_for_object(focused_object,
                                                                        focused_object_index);
        if (solid_faces) {
            scene_editor_material_preview_fill_sampled_triangle(renderer,
                                                                &projected[i],
                                                                focused_object,
                                                                &projector->viewport,
                                                                fill_color,
                                                                z_buffer,
                                                                projector->viewport.w,
                                                                has_texture);
        } else if (has_texture) {
            scene_editor_material_preview_fill_sampled_triangle(renderer,
                                                                &projected[i],
                                                                focused_object,
                                                                &projector->viewport,
                                                                fill_color,
                                                                NULL,
                                                                0,
                                                                true);
        } else {
            scene_editor_material_preview_fill_solid_triangle(renderer, &projected[i], fill_color);
        }
    }

    SDL_SetRenderDrawColor(renderer, edge_color.r, edge_color.g, edge_color.b, edge_color.a);
    if (solid_faces && z_buffer) {
        double depth_epsilon = fmax(0.05, projector->span_max * 0.025);
        scene_editor_material_preview_draw_solid_visible_edges(renderer,
                                                               projected,
                                                               projected_count,
                                                               &projector->viewport,
                                                               z_buffer,
                                                               projector->viewport.w,
                                                               depth_epsilon);
        scene_editor_material_preview_draw_selection_overlay(renderer,
                                                             projected,
                                                             projected_count,
                                                             &projector->viewport,
                                                             z_buffer,
                                                             projector->viewport.w,
                                                             selected_triangles,
                                                             selected_triangle_count,
                                                             depth_epsilon);
    } else {
        for (int i = 0; i < projected_count; ++i) {
            scene_editor_material_preview_draw_edges(renderer, &projected[i]);
        }
        scene_editor_material_preview_draw_selection_overlay(renderer,
                                                             projected,
                                                             projected_count,
                                                             &projector->viewport,
                                                             NULL,
                                                             0,
                                                             selected_triangles,
                                                             selected_triangle_count,
                                                             fmax(0.05, projector->span_max * 0.025));
    }

    free(z_buffer);
    RuntimeScene3D_Free(&scene);
    return true;
}

bool SceneEditorMaterialPreviewRenderFocusedObject(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int focused_object_index,
    bool solid_faces) {
    return SceneEditorMaterialPreviewRenderFocusedObjectWithSelection(renderer,
                                                                     projector,
                                                                     focused_object_index,
                                                                     solid_faces,
                                                                     NULL,
                                                                     0);
}
