#include "app/preview_retained_scene_surface.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "editor/scene_editor_primitive_preview_geometry.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "vk_renderer.h"

#define PREVIEW_RETAINED_SCENE_SURFACE_SCALE 0.75

typedef struct PreviewRetainedSceneSurfacePoint3 {
    double x;
    double y;
    double z;
} PreviewRetainedSceneSurfacePoint3;

typedef struct PreviewRetainedSceneSurfaceVertex {
    double x;
    double y;
    double depth;
} PreviewRetainedSceneSurfaceVertex;

typedef struct PreviewRetainedSceneSurfaceCache {
    VkRenderer* renderer;
    VkRendererTexture texture;
    bool texture_valid;
    uint8_t* rgba;
    double* depth;
    int width;
    int height;
    uint64_t signature;
    bool signature_valid;
    bool pixels_valid;
    PreviewRetainedSceneSurfaceStats stats;
} PreviewRetainedSceneSurfaceCache;

static PreviewRetainedSceneSurfaceCache g_preview_surface;

static uint64_t preview_retained_scene_surface_hash(uint64_t hash,
                                                    const void* bytes,
                                                    size_t size) {
    const uint8_t* data = (const uint8_t*)bytes;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= (uint64_t)data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static SDL_Color preview_retained_scene_surface_base_color(int scene_object_index) {
    static const SDL_Color k_palette[] = {
        {240, 240, 240, 255},
        {255, 190, 112, 255},
        {116, 194, 255, 255},
        {148, 226, 176, 255}};
    int index = scene_object_index;
    if (index >= 0 && index < sceneSettings.objectCount) {
        const SceneObject* object = &sceneSettings.sceneObjects[index];
        return (SDL_Color){
            SceneObjectColorR(object),
            SceneObjectColorG(object),
            SceneObjectColorB(object),
            SceneObjectAlphaByte(object)};
    }
    if (index < 0) index = 0;
    return k_palette[index % (int)(sizeof(k_palette) / sizeof(k_palette[0]))];
}

static uint64_t preview_retained_scene_surface_signature(
    const PreviewCameraProjector* projector,
    const PreviewRetainedSceneFrame* frame) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = preview_retained_scene_surface_hash(hash, projector, sizeof(*projector));
    hash = preview_retained_scene_surface_hash(hash, frame, sizeof(*frame));
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    hash = preview_retained_scene_surface_hash(hash, &seeds.valid, sizeof(seeds.valid));
    hash = preview_retained_scene_surface_hash(
        hash, &seeds.primitive_count, sizeof(seeds.primitive_count));
    for (int i = 0; seeds.valid && i < seeds.primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
        SDL_Color color = preview_retained_scene_surface_base_color(
            primitive->scene_object_index);
        hash = preview_retained_scene_surface_hash(
            hash, primitive, sizeof(*primitive));
        hash = preview_retained_scene_surface_hash(hash, &color, sizeof(color));
    }
    for (int i = 0; i < SceneEditorMeshPreviewStoreInstanceCount(); ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance =
            SceneEditorMeshPreviewStoreGetInstance(i);
        const CoreMeshAssetRuntimeContract* contract =
            instance ? SceneEditorMeshPreviewStoreGetContract(instance->asset_index)
                     : NULL;
        const CoreMeshPreviewLodMesh* lod =
            instance ? SceneEditorMeshPreviewStoreGetForQuality(
                           instance->asset_index, true)
                     : NULL;
        SDL_Color color = preview_retained_scene_surface_base_color(
            instance ? instance->scene_object_index : i);
        if (!instance || !contract || !lod) continue;
        hash = preview_retained_scene_surface_hash(
            hash, instance, sizeof(*instance));
        hash = preview_retained_scene_surface_hash(
            hash, contract, sizeof(*contract));
        hash = preview_retained_scene_surface_hash(
            hash, &lod->vertex_count, sizeof(lod->vertex_count));
        hash = preview_retained_scene_surface_hash(
            hash, &lod->triangle_count, sizeof(lod->triangle_count));
        hash = preview_retained_scene_surface_hash(hash, &color, sizeof(color));
    }
    return hash;
}

static PreviewRetainedSceneSurfacePoint3 preview_retained_scene_surface_rotate(
    PreviewRetainedSceneSurfacePoint3 point,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    const double cx = cos(instance->rotation_x);
    const double sx = sin(instance->rotation_x);
    const double cy = cos(instance->rotation_y);
    const double sy = sin(instance->rotation_y);
    const double cz = cos(instance->rotation_z);
    const double sz = sin(instance->rotation_z);
    double value = point.y * cx - point.z * sx;
    point.z = point.y * sx + point.z * cx;
    point.y = value;
    value = point.x * cy + point.z * sy;
    point.z = -point.x * sy + point.z * cy;
    point.x = value;
    {
        const double rotated_x = point.x * cz - point.y * sz;
        const double rotated_y = point.x * sz + point.y * cz;
        point.x = rotated_x;
        point.y = rotated_y;
    }
    return point;
}

static PreviewRetainedSceneSurfacePoint3 preview_retained_scene_surface_world(
    CoreObjectVec3 local,
    const CoreMeshAssetRuntimeContract* contract,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    PreviewRetainedSceneSurfacePoint3 pivot = {0.0, 0.0, 0.0};
    PreviewRetainedSceneSurfacePoint3 point;
    if (instance->rotation_pivot_policy ==
        RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM) {
        pivot = (PreviewRetainedSceneSurfacePoint3){
            instance->rotation_pivot_x * instance->scale_x,
            instance->rotation_pivot_y * instance->scale_y,
            instance->rotation_pivot_z * instance->scale_z};
    } else if (instance->rotation_pivot_policy ==
               RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER) {
        pivot = (PreviewRetainedSceneSurfacePoint3){
            (contract->local_bounds.min.x + contract->local_bounds.max.x) *
                0.5 * instance->scale_x,
            (contract->local_bounds.min.y + contract->local_bounds.max.y) *
                0.5 * instance->scale_y,
            (contract->local_bounds.min.z + contract->local_bounds.max.z) *
                0.5 * instance->scale_z};
    }
    point = (PreviewRetainedSceneSurfacePoint3){
        local.x * instance->scale_x - pivot.x,
        local.y * instance->scale_y - pivot.y,
        local.z * instance->scale_z - pivot.z};
    point = preview_retained_scene_surface_rotate(point, instance);
    point.x += pivot.x + instance->position_x;
    point.y += pivot.y + instance->position_y;
    point.z += pivot.z + instance->position_z;
    return point;
}

static PreviewRetainedSceneSurfacePoint3 preview_retained_scene_surface_normal(
    PreviewRetainedSceneSurfacePoint3 a,
    PreviewRetainedSceneSurfacePoint3 b,
    PreviewRetainedSceneSurfacePoint3 c) {
    const double ux = b.x - a.x;
    const double uy = b.y - a.y;
    const double uz = b.z - a.z;
    const double vx = c.x - a.x;
    const double vy = c.y - a.y;
    const double vz = c.z - a.z;
    return (PreviewRetainedSceneSurfacePoint3){
        uy * vz - uz * vy,
        uz * vx - ux * vz,
        ux * vy - uy * vx};
}

static bool preview_retained_scene_surface_project(
    const PreviewCameraProjector* projector,
    PreviewRetainedSceneSurfacePoint3 point,
    PreviewRetainedSceneSurfaceVertex* out_vertex) {
    double screen_x = 0.0;
    double screen_y = 0.0;
    double depth = 0.0;
    bool inside = false;
    if (!projector || !out_vertex ||
        !PreviewCameraProjectorProjectPoint(projector,
                                            point.x,
                                            point.y,
                                            point.z,
                                            &screen_x,
                                            &screen_y,
                                            &depth,
                                            &inside)) {
        return false;
    }
    (void)inside;
    out_vertex->x =
        (screen_x - (double)projector->viewport.x) *
        PREVIEW_RETAINED_SCENE_SURFACE_SCALE;
    out_vertex->y =
        (screen_y - (double)projector->viewport.y) *
        PREVIEW_RETAINED_SCENE_SURFACE_SCALE;
    out_vertex->depth = depth;
    return isfinite(out_vertex->x) && isfinite(out_vertex->y) &&
           isfinite(out_vertex->depth);
}

static double preview_retained_scene_surface_edge(double ax,
                                                  double ay,
                                                  double bx,
                                                  double by,
                                                  double px,
                                                  double py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void preview_retained_scene_surface_rasterize_triangle(
    const PreviewCameraProjector* projector,
    PreviewRetainedSceneSurfacePoint3 wa,
    PreviewRetainedSceneSurfacePoint3 wb,
    PreviewRetainedSceneSurfacePoint3 wc,
    SDL_Color base,
    const PreviewRetainedSceneFrame* frame,
    PreviewRetainedSceneSurfaceStats* stats) {
    PreviewRetainedSceneSurfaceVertex a;
    PreviewRetainedSceneSurfaceVertex b;
    PreviewRetainedSceneSurfaceVertex c;
    PreviewRetainedSceneSurfacePoint3 normal =
        preview_retained_scene_surface_normal(wa, wb, wc);
    PreviewRetainedSceneSurfacePoint3 center = {
        (wa.x + wb.x + wc.x) / 3.0,
        (wa.y + wb.y + wc.y) / 3.0,
        (wa.z + wb.z + wc.z) / 3.0};
    double area = 0.0;
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    SDL_Color color;

    if (!preview_retained_scene_surface_project(projector, wa, &a) ||
        !preview_retained_scene_surface_project(projector, wb, &b) ||
        !preview_retained_scene_surface_project(projector, wc, &c)) {
        return;
    }
    area = preview_retained_scene_surface_edge(a.x, a.y, b.x, b.y, c.x, c.y);
    if (!isfinite(area) || fabs(area) <= 1e-8) return;
    min_x = (int)floor(fmin(a.x, fmin(b.x, c.x)));
    max_x = (int)ceil(fmax(a.x, fmax(b.x, c.x)));
    min_y = (int)floor(fmin(a.y, fmin(b.y, c.y)));
    max_y = (int)ceil(fmax(a.y, fmax(b.y, c.y)));
    if (max_x < 0 || max_y < 0 ||
        min_x >= g_preview_surface.width || min_y >= g_preview_surface.height) {
        return;
    }
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= g_preview_surface.width) max_x = g_preview_surface.width - 1;
    if (max_y >= g_preview_surface.height) max_y = g_preview_surface.height - 1;
    color = PreviewRetainedSceneShadeColor(base,
                                           normal.x,
                                           normal.y,
                                           normal.z,
                                           center.x,
                                           center.y,
                                           center.z,
                                           frame);
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const double px = (double)x + 0.5;
            const double py = (double)y + 0.5;
            const double w0 =
                preview_retained_scene_surface_edge(b.x, b.y, c.x, c.y, px, py) /
                area;
            const double w1 =
                preview_retained_scene_surface_edge(c.x, c.y, a.x, a.y, px, py) /
                area;
            const double w2 = 1.0 - w0 - w1;
            const size_t pixel =
                (size_t)y * (size_t)g_preview_surface.width + (size_t)x;
            double depth = 0.0;
            if (w0 < -1e-4 || w1 < -1e-4 || w2 < -1e-4) continue;
            depth = w0 * a.depth + w1 * b.depth + w2 * c.depth;
            if (!(depth > 0.0) || depth >= g_preview_surface.depth[pixel]) continue;
            g_preview_surface.depth[pixel] = depth;
            g_preview_surface.rgba[pixel * 4u + 0u] = color.r;
            g_preview_surface.rgba[pixel * 4u + 1u] = color.g;
            g_preview_surface.rgba[pixel * 4u + 2u] = color.b;
            g_preview_surface.rgba[pixel * 4u + 3u] = color.a;
        }
    }
    stats->rendered_triangles += 1u;
}

static bool preview_retained_scene_surface_prepare_pixels(int width, int height) {
    const size_t pixels = (size_t)width * (size_t)height;
    if (width <= 0 || height <= 0 || pixels > SIZE_MAX / 4u) return false;
    if (g_preview_surface.width != width || g_preview_surface.height != height) {
        uint8_t* rgba = (uint8_t*)malloc(pixels * 4u);
        double* depth = (double*)malloc(pixels * sizeof(*depth));
        if (!rgba || !depth) {
            free(rgba);
            free(depth);
            return false;
        }
        free(g_preview_surface.rgba);
        free(g_preview_surface.depth);
        g_preview_surface.rgba = rgba;
        g_preview_surface.depth = depth;
        g_preview_surface.width = width;
        g_preview_surface.height = height;
    }
    memset(g_preview_surface.rgba, 0, pixels * 4u);
    for (size_t i = 0u; i < pixels; ++i) {
        g_preview_surface.depth[i] = INFINITY;
    }
    return true;
}

static bool preview_retained_scene_surface_upload(VkRenderer* renderer) {
    VkResult result;
    if (g_preview_surface.texture_valid &&
        (g_preview_surface.texture.width != (uint32_t)g_preview_surface.width ||
         g_preview_surface.texture.height != (uint32_t)g_preview_surface.height)) {
        vk_renderer_wait_idle(renderer);
        vk_renderer_texture_destroy(renderer, &g_preview_surface.texture);
        memset(&g_preview_surface.texture, 0, sizeof(g_preview_surface.texture));
        g_preview_surface.texture_valid = false;
    }
    if (!g_preview_surface.texture_valid) {
        result = vk_renderer_texture_create_from_rgba(
            renderer,
            g_preview_surface.rgba,
            (uint32_t)g_preview_surface.width,
            (uint32_t)g_preview_surface.height,
            VK_FILTER_LINEAR,
            &g_preview_surface.texture);
        g_preview_surface.texture_valid = result == VK_SUCCESS;
        return g_preview_surface.texture_valid;
    }
    return vk_renderer_texture_update_rgba_subrect(
               renderer,
               &g_preview_surface.texture,
               g_preview_surface.rgba,
               (size_t)g_preview_surface.width * 4u,
               0u,
               0u,
               (uint32_t)g_preview_surface.width,
               (uint32_t)g_preview_surface.height) == VK_SUCCESS;
}

static void preview_retained_scene_surface_rasterize(
    const PreviewCameraProjector* projector,
    const PreviewRetainedSceneFrame* frame,
    PreviewRetainedSceneSurfaceStats* stats) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    for (int i = 0; seeds.valid && i < seeds.primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
        SceneEditorPrimitivePreviewTriangle
            triangles[SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES];
        size_t triangle_count = 0u;
        if (!SceneEditorPrimitivePreviewBuildTriangles(
                primitive, triangles, &triangle_count)) {
            continue;
        }
        for (size_t triangle = 0u; triangle < triangle_count; ++triangle) {
            preview_retained_scene_surface_rasterize_triangle(
                projector,
                (PreviewRetainedSceneSurfacePoint3){
                    triangles[triangle].a.x,
                    triangles[triangle].a.y,
                    triangles[triangle].a.z},
                (PreviewRetainedSceneSurfacePoint3){
                    triangles[triangle].b.x,
                    triangles[triangle].b.y,
                    triangles[triangle].b.z},
                (PreviewRetainedSceneSurfacePoint3){
                    triangles[triangle].c.x,
                    triangles[triangle].c.y,
                    triangles[triangle].c.z},
                preview_retained_scene_surface_base_color(
                    primitive->scene_object_index),
                frame,
                stats);
        }
        stats->rendered_instances += 1;
    }
    for (int i = 0; i < SceneEditorMeshPreviewStoreInstanceCount(); ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance =
            SceneEditorMeshPreviewStoreGetInstance(i);
        const CoreMeshAssetRuntimeContract* contract =
            instance ? SceneEditorMeshPreviewStoreGetContract(instance->asset_index)
                     : NULL;
        const CoreMeshPreviewLodMesh* lod =
            instance ? SceneEditorMeshPreviewStoreGetForQuality(
                           instance->asset_index, true)
                     : NULL;
        if (!instance || !contract || !lod) continue;
        for (size_t triangle = 0u; triangle < lod->triangle_count; ++triangle) {
            const uint32_t ia = lod->indices[triangle * 3u + 0u];
            const uint32_t ib = lod->indices[triangle * 3u + 1u];
            const uint32_t ic = lod->indices[triangle * 3u + 2u];
            if (ia >= lod->vertex_count || ib >= lod->vertex_count ||
                ic >= lod->vertex_count) {
                continue;
            }
            preview_retained_scene_surface_rasterize_triangle(
                projector,
                preview_retained_scene_surface_world(
                    lod->vertices[ia], contract, instance),
                preview_retained_scene_surface_world(
                    lod->vertices[ib], contract, instance),
                preview_retained_scene_surface_world(
                    lod->vertices[ic], contract, instance),
                preview_retained_scene_surface_base_color(
                    instance->scene_object_index),
                frame,
                stats);
        }
        stats->rendered_instances += 1;
    }
}

void PreviewRetainedSceneSurfacePrepare(void) {
    SceneEditorMeshPreviewStorePrepare(ray_tracing_runtime_mesh_assets_last());
    g_preview_surface.signature_valid = false;
    g_preview_surface.pixels_valid = false;
}

bool PreviewRetainedSceneSurfaceRender(
    SDL_Renderer* renderer,
    const PreviewCameraProjector* projector,
    const PreviewRetainedSceneFrame* frame,
    PreviewRetainedSceneSurfaceStats* out_stats) {
    VkRenderer* vk = (VkRenderer*)renderer;
    uint64_t signature = 0u;
    SDL_Rect destination;
    if (out_stats) memset(out_stats, 0, sizeof(*out_stats));
    if (!renderer || !projector || !frame ||
        !PreviewRetainedSceneQualityUsesSurface(frame->quality) ||
        RayEvaluatedSceneSnapshotValidate(&frame->evaluated_scene) !=
            TIMELINE_STATUS_OK) {
        return false;
    }
    if (g_preview_surface.texture_valid && g_preview_surface.renderer != vk) {
        return false;
    }
    signature = preview_retained_scene_surface_signature(projector, frame);
    if (!g_preview_surface.signature_valid ||
        signature != g_preview_surface.signature ||
        !g_preview_surface.pixels_valid) {
        PreviewRetainedSceneSurfaceStats stats = {0};
        stats.quality = frame->quality;
        if (!preview_retained_scene_surface_prepare_pixels(
                (int)ceil((double)projector->viewport.w *
                          PREVIEW_RETAINED_SCENE_SURFACE_SCALE),
                (int)ceil((double)projector->viewport.h *
                          PREVIEW_RETAINED_SCENE_SURFACE_SCALE))) {
            return false;
        }
        preview_retained_scene_surface_rasterize(projector, frame, &stats);
        if (stats.rendered_instances <= 0 ||
            !preview_retained_scene_surface_upload(vk)) {
            return false;
        }
        g_preview_surface.renderer = vk;
        g_preview_surface.signature = signature;
        g_preview_surface.signature_valid = true;
        g_preview_surface.pixels_valid = true;
        g_preview_surface.stats = stats;
    }
    destination = projector->viewport;
    vk_renderer_draw_texture(vk, &g_preview_surface.texture, NULL, &destination);
    if (out_stats) *out_stats = g_preview_surface.stats;
    return true;
}

void PreviewRetainedSceneSurfaceReset(SDL_Renderer* renderer) {
    VkRenderer* vk = (VkRenderer*)renderer;
    if (g_preview_surface.texture_valid && vk &&
        (!g_preview_surface.renderer || g_preview_surface.renderer == vk)) {
        vk_renderer_wait_idle(vk);
        vk_renderer_texture_destroy(vk, &g_preview_surface.texture);
    }
    free(g_preview_surface.rgba);
    free(g_preview_surface.depth);
    memset(&g_preview_surface, 0, sizeof(g_preview_surface));
}
