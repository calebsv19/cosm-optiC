#include "render/pipeline/ray_tracing2_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/scene_editor.h"
#include "geo/shape_adapter.h"
#include "geo/shape_asset.h"
#include "import/fluid_import.h"
#include "import/shape_import.h"
#include "render/fluid/fluid_state.h"
#include "render/fluid_overlay.h"

static FluidManifest g_fluidManifest = {0};
static FluidFrame g_fluidFrame = {0};
static int g_loadedFrameIndex = -1;
static FluidGridBounds g_grid = {0};
static bool g_manifestLoaded = false;
static CoreSpaceDesc g_fluidSpaceDesc = {0};
static bool g_fluidSpaceValid = false;

static bool RayTracing2Fluid_LoadShapelibReplacement(const char *asset_path, ShapeAsset *asset) {
    if (!asset) return false;
    char base[256] = {0};
    const char *path = asset_path ? asset_path : asset->name;
    if (path) {
        const char *slash = strrchr(path, '/');
        const char *fname = slash ? slash + 1 : path;
        strncpy(base, fname, sizeof(base) - 1);
    }
    if (base[0] == '\0' && asset->name) {
        const char *slash = strrchr(asset->name, '/');
        const char *fname = slash ? slash + 1 : asset->name;
        strncpy(base, fname, sizeof(base) - 1);
    }
    if (base[0] == '\0') return false;
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    const char *candidates[] = {
        "import", "../line_drawing/export", "../physics_sim/import"
    };
    char tryPath[512];
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        snprintf(tryPath, sizeof(tryPath), "%s/%s.json", candidates[i], base);
        ShapeDocument doc = {0};
        if (shape_import_load(tryPath, &doc) && doc.shapeCount > 0) {
            ShapeAsset replacement = {0};
            if (shape_asset_from_shapelib_shape(&doc.shapes[0], 0.1f, &replacement)) {
                shape_asset_free(asset);
                *asset = replacement;
                ShapeDocument_Free(&doc);
                return true;
            }
        }
        ShapeDocument_Free(&doc);
    }
    return false;
}

static void RayTracing2Fluid_AddImportedObject(const FluidImportShape *imp) {
    if (!imp || !imp->path) return;
    double min_x = g_grid.valid ? g_grid.min_x : 0.0;
    double min_y = g_grid.valid ? g_grid.min_y : 0.0;
    double max_x = g_grid.valid ? g_grid.max_x : (double)sceneSettings.windowWidth;
    double max_y = g_grid.valid ? g_grid.max_y : (double)sceneSettings.windowHeight;
    double world_x = min_x + (max_x - min_x) * imp->pos_x_norm;
    double world_y = min_y + (max_y - min_y) * imp->pos_y_norm;

    ShapeAsset asset = {0};
    bool loaded = shape_asset_load_file(imp->path, &asset);
    if (loaded) {
        RayTracing2Fluid_LoadShapelibReplacement(imp->path, &asset);
    }
    double angle = imp->rotation_deg * M_PI / 180.0;
    double scale = (imp->scale > 0.0f) ? imp->scale : 1.0;
    if (g_fluidSpaceValid) {
        float asset_max_dim = 1.0f;
        if (loaded) {
            ShapeAssetBounds bnds;
            if (shape_asset_bounds(&asset, &bnds) && bnds.valid) {
                float dx = bnds.max_x - bnds.min_x;
                float dy = bnds.max_y - bnds.min_y;
                asset_max_dim = (dx > dy) ? dx : dy;
                if (asset_max_dim <= 0.0001f) asset_max_dim = 1.0f;
            }
        }
        CoreSpaceImport import_in;
        CoreSpaceWorldTransform world_out;
        memset(&import_in, 0, sizeof(import_in));
        memset(&world_out, 0, sizeof(world_out));
        import_in.pos_x_raw = imp->pos_x_norm;
        import_in.pos_y_raw = imp->pos_y_norm;
        import_in.rotation_deg = imp->rotation_deg;
        import_in.scale = (imp->scale > 0.0f) ? imp->scale : 1.0f;
        import_in.asset_max_dim = asset_max_dim;
        if (core_space_import_to_world(&g_fluidSpaceDesc, &import_in, &world_out).code == CORE_OK) {
            world_x = world_out.x;
            world_y = world_out.y;
            scale = world_out.scale;
        }
    }
    if (loaded) {
        int before = sceneSettings.objectCount;
        ShapeToSceneOptions opts = {.scale = scale, .offset_x = world_x, .offset_y = world_y};
        shape_asset_append_to_scene(&asset, &opts);
        int after = sceneSettings.objectCount;
        for (int i = before; i < after; ++i) {
            SceneObject *obj = &sceneSettings.sceneObjects[i];
            obj->rotation = angle;
            obj->dirty = true;
        }
        shape_asset_free(&asset);
    } else {
        if (sceneSettings.objectCount >= MAX_OBJECTS) return;
        SceneObject *obj = &sceneSettings.sceneObjects[sceneSettings.objectCount++];
        InitObject(obj, OBJECT_POLYGON, 0, 0, 0, 0, NULL, 0);
        strncpy(obj->type, "polygon", sizeof(obj->type) - 1);
        obj->x = world_x;
        obj->y = world_y;
        obj->rotation = angle;
        obj->scale = scale;
        obj->dirty = true;
        printf("[fluid] warning: could not load asset %s\n", imp->path);
    }
}

bool RayTracing2Fluid_InitializeScene(void) {
    g_manifestLoaded = false;
    g_fluidSpaceValid = false;
    g_grid.valid = false;

    if (AnimationUseFluidScene()) {
        if (fluid_manifest_load(animSettings.fluidManifest, &g_fluidManifest)) {
            printf("[fluid] Loaded manifest: %zu frames (%ux%u)\n",
                   g_fluidManifest.count,
                   g_fluidManifest.grid_w,
                   g_fluidManifest.grid_h);
            g_grid.valid = true;
            g_grid.min_x = g_fluidManifest.origin_x;
            g_grid.min_y = g_fluidManifest.origin_y;
            g_grid.max_x = g_fluidManifest.origin_x +
                           g_fluidManifest.cell_size * (float)g_fluidManifest.grid_w;
            g_grid.max_y = g_fluidManifest.origin_y +
                           g_fluidManifest.cell_size * (float)g_fluidManifest.grid_h;
            g_manifestLoaded = true;
            if (core_space_desc_default_from_grid((int)g_fluidManifest.grid_w,
                                                  (int)g_fluidManifest.grid_h,
                                                  g_fluidManifest.origin_x,
                                                  g_fluidManifest.origin_y,
                                                  g_fluidManifest.cell_size,
                                                  &g_fluidSpaceDesc).code == CORE_OK) {
                if (g_fluidManifest.space_author_window_w > 0) {
                    g_fluidSpaceDesc.author_window_w = g_fluidManifest.space_author_window_w;
                }
                if (g_fluidManifest.space_author_window_h > 0) {
                    g_fluidSpaceDesc.author_window_h = g_fluidManifest.space_author_window_h;
                }
                if (g_fluidManifest.space_desired_fit > 0.0f) {
                    g_fluidSpaceDesc.desired_fit = g_fluidManifest.space_desired_fit;
                }
                g_fluidSpaceValid = true;
            }
            double grid_w_world = g_grid.max_x - g_grid.min_x;
            double grid_h_world = g_grid.max_y - g_grid.min_y;
            sceneSettings.camera.x = g_grid.min_x + grid_w_world * 0.5;
            sceneSettings.camera.y = g_grid.min_y + grid_h_world * 0.5;
            double zoom_x = (grid_w_world > 1e-4)
                                ? ((double)sceneSettings.windowWidth / grid_w_world)
                                : 1.0;
            double zoom_y = (grid_h_world > 1e-4)
                                ? ((double)sceneSettings.windowHeight / grid_h_world)
                                : 1.0;
            sceneSettings.camera.zoom = fmin(zoom_x, zoom_y) * 0.9;
            sceneSettings.objectCount = 0;
            if (g_fluidManifest.count > 0) {
                int idx = g_fluidFrameIndex;
                if (idx < 0) idx = 0;
                if (idx >= (int)g_fluidManifest.count) idx = (int)g_fluidManifest.count - 1;
                const char *path = g_fluidManifest.paths[idx];
                if (path && fluid_frame_load(path, &g_fluidFrame)) {
                    g_fluidFrameIndex = idx;
                    g_loadedFrameIndex = idx;
                    printf("[fluid] Loaded frame %d from %s\n", idx, path);
                }
            }
            for (size_t i = 0; i < g_fluidManifest.import_count; ++i) {
                RayTracing2Fluid_AddImportedObject(&g_fluidManifest.imports[i]);
            }
            return true;
        }
        printf("[fluid] Failed to load manifest: %s\n", animSettings.fluidManifest);
        return false;
    }

    if (animSettings.fluidManifest[0] == '\0' &&
        strlen(animSettings.frameDir) > 0 &&
        (strstr(animSettings.frameDir, ".vf2d") || strstr(animSettings.frameDir, ".pack"))) {
        const char *path = animSettings.frameDir;
        if (fluid_frame_load_single(path, &g_fluidFrame)) {
            g_loadedFrameIndex = 0;
            g_grid.valid = true;
            g_grid.min_x = g_grid.min_y = 0.0f;
            g_grid.max_x = (float)g_fluidFrame.w;
            g_grid.max_y = (float)g_fluidFrame.h;
            printf("[fluid] Loaded single frame from %s\n", path);
            return true;
        }
    }
    return false;
}

void RayTracing2Fluid_CleanupScene(void) {
    fluid_frame_free(&g_fluidFrame);
    fluid_manifest_free(&g_fluidManifest);
    g_fluidSpaceValid = false;
    g_manifestLoaded = false;
    g_grid.valid = false;
    g_loadedFrameIndex = -1;
}

void RayTracing2Fluid_ClampCameraToGrid(void) {
    if (!g_grid.valid) return;
    if (sceneSettings.camera.x < g_grid.min_x) sceneSettings.camera.x = g_grid.min_x;
    if (sceneSettings.camera.x > g_grid.max_x) sceneSettings.camera.x = g_grid.max_x;
    if (sceneSettings.camera.y < g_grid.min_y) sceneSettings.camera.y = g_grid.min_y;
    if (sceneSettings.camera.y > g_grid.max_y) sceneSettings.camera.y = g_grid.max_y;
}

void RayTracing2Fluid_RenderOverlay(SDL_Renderer* renderer, int width, int height) {
    (void)width;
    (void)height;
    if (!g_fluidOverlayEnabled || g_fluidManifest.count <= 0) {
        return;
    }
    int idx = g_fluidFrameIndex;
    if (idx < 0) idx = 0;
    if (idx >= (int)g_fluidManifest.count) idx = (int)g_fluidManifest.count - 1;
    if (idx != g_loadedFrameIndex) {
        fluid_frame_free(&g_fluidFrame);
        const char *path = g_fluidManifest.paths[idx];
        if (path && fluid_frame_load(path, &g_fluidFrame)) {
            g_loadedFrameIndex = idx;
            printf("[fluid] Loaded frame %d from %s\n", idx, path);
        }
    }
    if (g_fluidFrame.density) {
        fluid_overlay_draw(renderer, &g_fluidFrame, &sceneSettings.camera, width, height);
    }
}
