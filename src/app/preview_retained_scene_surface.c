#include "app/preview_retained_scene_surface.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "app/preview_mesh_instance_bounds.h"
#include "config/config_manager.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "editor/scene_editor_primitive_preview_geometry.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_bridge.h"
#include "vk_renderer.h"

#define PREVIEW_RETAINED_SCENE_SURFACE_SCALE 0.75

typedef PreviewMeshInstancePoint3 PreviewRetainedSceneSurfacePoint3;

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
        const CoreMeshAssetBounds3* bounds =
            instance ? SceneEditorMeshPreviewStoreGetBounds(instance->asset_index)
                     : NULL;
        const CoreMeshPreviewLodMesh* lod =
            instance ? SceneEditorMeshPreviewStoreGetForQuality(
                           instance->asset_index, true)
                     : NULL;
        SDL_Color color = preview_retained_scene_surface_base_color(
            instance ? instance->scene_object_index : i);
        if (!instance || !bounds) continue;
        hash = preview_retained_scene_surface_hash(
            hash, instance, sizeof(*instance));
        hash = preview_retained_scene_surface_hash(
            hash, bounds, sizeof(*bounds));
        if (lod) {
            hash = preview_retained_scene_surface_hash(
                hash, &lod->vertex_count, sizeof(lod->vertex_count));
            hash = preview_retained_scene_surface_hash(
                hash, &lod->triangle_count, sizeof(lod->triangle_count));
        }
        hash = preview_retained_scene_surface_hash(hash, &color, sizeof(color));
    }
    return hash;
}

static PreviewRetainedSceneSurfacePoint3 preview_retained_scene_surface_world(
    CoreObjectVec3 local,
    const CoreMeshAssetBounds3* bounds,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    PreviewRetainedSceneSurfacePoint3 point = {0.0, 0.0, 0.0};
    (void)PreviewMeshInstanceTransformPoint(local, bounds, instance, &point);
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

static void preview_retained_scene_surface_rasterize_line(
    const PreviewCameraProjector* projector,
    PreviewRetainedSceneSurfacePoint3 wa,
    PreviewRetainedSceneSurfacePoint3 wb,
    SDL_Color color) {
    PreviewRetainedSceneSurfaceVertex a;
    PreviewRetainedSceneSurfaceVertex b;
    double dx = 0.0;
    double dy = 0.0;
    int steps = 0;
    if (!preview_retained_scene_surface_project(projector, wa, &a) ||
        !preview_retained_scene_surface_project(projector, wb, &b)) {
        return;
    }
    dx = b.x - a.x;
    dy = b.y - a.y;
    steps = (int)ceil(fmax(fabs(dx), fabs(dy)));
    if (steps < 1) steps = 1;
    for (int step = 0; step <= steps; ++step) {
        const double t = (double)step / (double)steps;
        const int center_x = (int)lround(a.x + dx * t);
        const int center_y = (int)lround(a.y + dy * t);
        const double depth = a.depth + (b.depth - a.depth) * t;
        if (!(depth > 0.0)) continue;
        for (int oy = -1; oy <= 1; ++oy) {
            for (int ox = -1; ox <= 1; ++ox) {
                const int x = center_x + ox;
                const int y = center_y + oy;
                size_t pixel = 0u;
                if (x < 0 || y < 0 ||
                    x >= g_preview_surface.width ||
                    y >= g_preview_surface.height) {
                    continue;
                }
                pixel = (size_t)y * (size_t)g_preview_surface.width +
                        (size_t)x;
                if (depth >= g_preview_surface.depth[pixel]) continue;
                g_preview_surface.depth[pixel] = depth;
                g_preview_surface.rgba[pixel * 4u + 0u] = color.r;
                g_preview_surface.rgba[pixel * 4u + 1u] = color.g;
                g_preview_surface.rgba[pixel * 4u + 2u] = color.b;
                g_preview_surface.rgba[pixel * 4u + 3u] = color.a;
            }
        }
    }
}

static void preview_retained_scene_surface_rasterize_bounds(
    const PreviewCameraProjector* projector,
    const CoreMeshAssetBounds3* bounds,
    const RayTracingRuntimeMeshAssetInstance* instance,
    SDL_Color base) {
    static const int k_edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    PreviewMeshInstancePoint3 corners[8];
    SDL_Color color = {
        (Uint8)((int)base.r + (255 - (int)base.r) * 3 / 4),
        (Uint8)((int)base.g + (255 - (int)base.g) * 3 / 4),
        (Uint8)((int)base.b + (255 - (int)base.b) * 3 / 4),
        255};
    if (!PreviewMeshInstanceBuildBoundsCorners(bounds, instance, corners)) return;
    for (int edge = 0; edge < 12; ++edge) {
        preview_retained_scene_surface_rasterize_line(
            projector,
            corners[k_edges[edge][0]],
            corners[k_edges[edge][1]],
            color);
    }
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
        const CoreMeshAssetBounds3* bounds =
            instance ? SceneEditorMeshPreviewStoreGetBounds(instance->asset_index)
                     : NULL;
        const CoreMeshPreviewLodMesh* lod =
            instance ? SceneEditorMeshPreviewStoreGetForQuality(
                           instance->asset_index, true)
                     : NULL;
        if (!instance || !bounds) continue;
        if (!lod) {
            preview_retained_scene_surface_rasterize_bounds(
                projector,
                bounds,
                instance,
                preview_retained_scene_surface_base_color(
                    instance->scene_object_index));
            stats->bounds_fallback_instances += 1;
            stats->rendered_instances += 1;
            continue;
        }
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
                    lod->vertices[ia], bounds, instance),
                preview_retained_scene_surface_world(
                    lod->vertices[ib], bounds, instance),
                preview_retained_scene_surface_world(
                    lod->vertices[ic], bounds, instance),
                preview_retained_scene_surface_base_color(
                    instance->scene_object_index),
                frame,
                stats);
        }
        stats->mesh_lod_instances += 1;
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
