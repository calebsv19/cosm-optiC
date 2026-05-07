#include "render/ray_tracing2.h"
#include "config/config_manager.h"  // Include animation.h to access `sceneObjects[]` and `objectCount`
#include "render/render_helper.h"
#include "render/fast_rng.h"
#include "render/uniform_grid.h"
#include "render/ray_types.h"
#include "render/integrator_common.h"
#include "render/material_bsdf.h"
#include "render/ray_tracing2_preview.h"
#include "render/pipeline/ray_tracing2_native3d_overlay.h"
#include "render/pipeline/ray_tracing2_preview_present.h"
#include "render/surface_mesh.h"
#include "render/integrators/hybrid/camera_path_integrator.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/integrators/hybrid/integrator_direct.h"
#include "render/integrators/hybrid/integrator_indirect.h"
#include "render/integrators/forward_light_integrator.h"
#include "render/integrators/direct_light_integrator.h"
#include "render/irradiance_cache.h"
#include "render/timer_hud_api.h"
#include "render/fluid_overlay.h"
#include "import/fluid_import.h"
#include "core_space.h"
#include "engine/Render/render_pipeline.h"
#include "render/fluid/fluid_state.h"
#include "editor/scene_editor.h"
#include "app/animation.h"
#include "app/runtime_time.h"
#include "camera/camera.h"
#include "render/space_mode_adapter.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_resolution.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_samples.h"
#include "geo/shape_asset.h"
#include "geo/shape_adapter.h"
#include "import/shape_import.h"
#include <stdio.h>   
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include "vk_renderer.h"

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif


// Global light source.
static Circle light = {100, 100, 10};  // Default position (updated dynamically)
Uint8* pixelBuffer = NULL;  // Global but uninitialized
static Uint8* tilePreviewBuffer = NULL;
static Uint8* native3DRenderBuffer = NULL;
static Uint8* native3DPreviewBuffer = NULL;
static size_t native3DRenderBufferCapacity = 0u;
static size_t native3DPreviewBufferCapacity = 0u;
static int native3DPreviewWidth = 0;
static int native3DPreviewHeight = 0;
float* energyBuffer = NULL;
float* directEnergyBufferCPU = NULL;
static uint32_t s_native3DSampleSequence = 1U;
static TileGrid tileGrid = {0};
static UniformGrid uniformGrid = {0};
static IrradianceCache irradianceCache = {0};
static MaterialBSDF* materialTable = NULL;
static int materialCapacity = 0;
static SurfaceMesh surfaceMesh = {0};
static TriangleMesh triangleMesh = {0};

static float* reflectionForwardBuffer = NULL;
static bool BuildReflectionCache(const IntegratorContext* ctx,
                                 const LightSource* light);
static int BuildMaterialTable(void);

static bool EnsureNative3DRenderBuffer(size_t pixel_count) {
    Uint8* resized = NULL;
    size_t byte_count = 0u;
    if (pixel_count == 0u) {
        return false;
    }
    if (native3DRenderBuffer && native3DRenderBufferCapacity >= pixel_count) {
        return true;
    }
    byte_count = pixel_count * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    resized = (Uint8*)realloc(native3DRenderBuffer, byte_count * sizeof(Uint8));
    if (!resized) {
        return false;
    }
    native3DRenderBuffer = resized;
    native3DRenderBufferCapacity = pixel_count;
    return true;
}

static bool EnsureNative3DPreviewBuffer(size_t pixel_count) {
    Uint8* resized = NULL;
    size_t byte_count = 0u;
    if (pixel_count == 0u) {
        return false;
    }
    if (native3DPreviewBuffer && native3DPreviewBufferCapacity >= pixel_count) {
        return true;
    }
    byte_count = pixel_count * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    resized = (Uint8*)realloc(native3DPreviewBuffer, byte_count * sizeof(Uint8));
    if (!resized) {
        return false;
    }
    native3DPreviewBuffer = resized;
    native3DPreviewBufferCapacity = pixel_count;
    return true;
}

static RuntimeNative3DSamplingContext NextNative3DSamplingContext(void) {
    RuntimeNative3DSamplingContext sampling = {0};
    sampling.sampleSequence = s_native3DSampleSequence++;
    if (sampling.sampleSequence == 0U) {
        sampling.sampleSequence = s_native3DSampleSequence++;
    }
    return sampling;
}

static int ResolveNative3DTemporalFrames(RayTracing3DIntegratorId integrator_id) {
    int frames = animSettings.temporalFrames3D;
    if (integrator_id == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT) {
        return 1;
    }
    if (frames < RUNTIME_3D_TEMPORAL_FRAMES_MIN) {
        frames = RUNTIME_3D_TEMPORAL_FRAMES_MIN;
    }
    if (frames > RUNTIME_3D_TEMPORAL_FRAMES_MAX) {
        frames = RUNTIME_3D_TEMPORAL_FRAMES_MAX;
    }
    return frames;
}

static void LogNative3DRenderStatsIfNeeded(RayTracing3DIntegratorId integrator_id,
                                           const RuntimeNative3DRenderStats* stats) {
    double avg_bounce = 0.0;
    if (!stats) return;
    if (integrator_id != RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE) return;
    if (stats->bouncePixelCount > 0) {
        avg_bounce = stats->totalBounceRadiance / (double)stats->bouncePixelCount;
    }
    printf("[native3d] diffuse frame hits=%d visible=%d bounce_pixels=%d secondary=%d hits2=%d lit2=%d max_r=%.4f max_b=%.4f avg_b=%.4f\n",
           stats->hitPixelCount,
           stats->visiblePixelCount,
           stats->bouncePixelCount,
           stats->secondaryRayCount,
           stats->secondaryHitCount,
           stats->secondaryContributingHitCount,
           stats->maxRadiance,
           stats->maxBounceRadiance,
           avg_bounce);
}

static FluidManifest g_fluidManifest = {0};
static FluidFrame g_fluidFrame = {0};
static int g_loadedFrameIndex = -1;
static FluidGridBounds g_grid = {0};
static bool g_manifestLoaded = false;
static CoreSpaceDesc g_fluidSpaceDesc = {0};
static bool g_fluidSpaceValid = false;

bool ExportCurrentNative3DFrameBMP(const char* filename) {
    int exportWidth = native3DPreviewWidth > 0 ? native3DPreviewWidth : sceneSettings.windowWidth;
    int exportHeight = native3DPreviewHeight > 0 ? native3DPreviewHeight : sceneSettings.windowHeight;
    return RayTracing2Native3DOverlay_ExportFrameBMP(filename,
                                                     exportWidth,
                                                     exportHeight,
                                                     native3DPreviewBuffer,
                                                     pixelBuffer);
}

static bool LoadShapelibReplacement(const char *asset_path, ShapeAsset *asset) {
    if (!asset) return false;
    // Derive base name (without extension)
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

static void AddImportedObject(const FluidImportShape *imp) {
    if (!imp || !imp->path) return;
    // Position in world using normalized coords against grid bounds.
    double min_x = g_grid.valid ? g_grid.min_x : 0.0;
    double min_y = g_grid.valid ? g_grid.min_y : 0.0;
    double max_x = g_grid.valid ? g_grid.max_x : (double)sceneSettings.windowWidth;
    double max_y = g_grid.valid ? g_grid.max_y : (double)sceneSettings.windowHeight;
    double world_x = min_x + (max_x - min_x) * imp->pos_x_norm;
    double world_y = min_y + (max_y - min_y) * imp->pos_y_norm;

    ShapeAsset asset = {0};
    bool loaded = shape_asset_load_file(imp->path, &asset);
    if (loaded) {
        LoadShapelibReplacement(imp->path, &asset);
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
        // Fallback: place a single polygon placeholder
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

static void ApplyMaterialOverrides(MaterialBSDF* material) {
    if (!material) return;
    if (animSettings.bsdfModel == 0) {
        material->specWeight = 0.0;
        material->diffuseWeight = 1.0;
        material->weightSum = 1.0;
    } else {
        material->weightSum = material->diffuseWeight + material->specWeight;
        if (material->weightSum <= 1e-4) {
            material->diffuseWeight = 1.0;
            material->specWeight = 0.0;
            material->weightSum = 1.0;
        }
    }
}

void InitRayTracingScene(void) {
    printf("DEBUG: Initializing Ray Tracing Scene\n");

    if (sceneSettings.objectCount == 0) {
        printf("WARNING: No scene objects detected! Rendering empty scene.\n");
    } else {
        printf("INFO: Loaded %d scene objects for ray tracing.\n", sceneSettings.objectCount);
        for (int i = 0; i < sceneSettings.objectCount; i++) {
            printf("  → Object %d: Type: %s, X: %.2f, Y: %.2f\n",
                   i, sceneSettings.sceneObjects[i].type,
                   (double)sceneSettings.sceneObjects[i].x,
                   (double)sceneSettings.sceneObjects[i].y);
        }
    }

    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    size_t pixelCount = (size_t)WIDTH * (size_t)HEIGHT;

    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate memory for pixel buffer!\n");
            exit(1);
        }
    }
    if (tilePreviewBuffer == NULL) {
        tilePreviewBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!tilePreviewBuffer) {
            printf("ERROR: Failed to allocate memory for tile preview buffer!\n");
            exit(1);
        }
    }

    if (energyBuffer == NULL) {
        energyBuffer = (float*)malloc(pixelCount * sizeof(float));
        if (!energyBuffer) {
            printf("ERROR: Failed to allocate memory for energy buffer!\n");
            exit(1);
        }
    }

    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
    memset(tilePreviewBuffer, 0, pixelCount * sizeof(Uint8));
    memset(energyBuffer, 0, pixelCount * sizeof(float));

    if (sceneSettings.bezierPath.numPoints > 0) {
        light.x = sceneSettings.bezierPath.points[0].x;
        light.y = sceneSettings.bezierPath.points[0].y;
        printf("INFO: Light source initialized from Bézier Path at (%.2f, %.2f)\n", light.x, light.y);
    } else {
        light.x = 100;
        light.y = 100;
        printf("WARNING: Bézier Path is uninitialized! Using default light position (100, 100)\n");
    }

    SurfaceMeshInit(&surfaceMesh);
    TriangleMeshInit(&triangleMesh);

    // Load fluid manifest if specified and enabled.
    if (AnimationUseFluidScene()) {
        if (fluid_manifest_load(animSettings.fluidManifest, &g_fluidManifest)) {
            printf("[fluid] Loaded manifest: %zu frames (%ux%u)\n",
                   g_fluidManifest.count,
                   g_fluidManifest.grid_w,
                   g_fluidManifest.grid_h);
            g_grid.valid = true;
            g_grid.min_x = g_fluidManifest.origin_x;
            g_grid.min_y = g_fluidManifest.origin_y;
            g_grid.max_x = g_fluidManifest.origin_x + g_fluidManifest.cell_size * (float)g_fluidManifest.grid_w;
            g_grid.max_y = g_fluidManifest.origin_y + g_fluidManifest.cell_size * (float)g_fluidManifest.grid_h;
            g_manifestLoaded = true;
            g_fluidSpaceValid = false;
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
            // Fit camera to grid bounds
            double grid_w_world = g_grid.max_x - g_grid.min_x;
            double grid_h_world = g_grid.max_y - g_grid.min_y;
            sceneSettings.camera.x = g_grid.min_x + grid_w_world * 0.5;
            sceneSettings.camera.y = g_grid.min_y + grid_h_world * 0.5;
            double zoom_x = (grid_w_world > 1e-4) ? ((double)sceneSettings.windowWidth / grid_w_world) : 1.0;
            double zoom_y = (grid_h_world > 1e-4) ? ((double)sceneSettings.windowHeight / grid_h_world) : 1.0;
            sceneSettings.camera.zoom = fmin(zoom_x, zoom_y) * 0.9;
            sceneSettings.objectCount = 0;
            // Preload first frame if available.
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

            // Populate imported objects from manifest.
            for (size_t i = 0; i < g_fluidManifest.import_count; ++i) {
                AddImportedObject(&g_fluidManifest.imports[i]);
            }
        } else {
            printf("[fluid] Failed to load manifest: %s\n", animSettings.fluidManifest);
            g_fluidSpaceValid = false;
        }
    } else if (animSettings.fluidManifest[0] == '\0' &&
               strlen(animSettings.frameDir) > 0 &&
               (strstr(animSettings.frameDir, ".vf2d") || strstr(animSettings.frameDir, ".pack"))) {
        // If a direct frame path was provided in config, attempt single-frame load.
        const char *path = animSettings.frameDir;
        if (fluid_frame_load_single(path, &g_fluidFrame)) {
            g_loadedFrameIndex = 0;
            g_grid.valid = true;
            g_grid.min_x = g_grid.min_y = 0.0f;
            g_grid.max_x = (float)g_fluidFrame.w;
            g_grid.max_y = (float)g_fluidFrame.h;
            printf("[fluid] Loaded single frame from %s\n", path);
        }
        g_fluidSpaceValid = false;
    } else {
        g_fluidSpaceValid = false;
    }
}

void CleanupRayTracing(void) {
    if (pixelBuffer != NULL) {
        free(pixelBuffer);
        pixelBuffer = NULL;
    }
    if (tilePreviewBuffer != NULL) {
        free(tilePreviewBuffer);
        tilePreviewBuffer = NULL;
    }
    if (native3DRenderBuffer != NULL) {
        free(native3DRenderBuffer);
        native3DRenderBuffer = NULL;
        native3DRenderBufferCapacity = 0u;
    }
    if (native3DPreviewBuffer != NULL) {
        free(native3DPreviewBuffer);
        native3DPreviewBuffer = NULL;
        native3DPreviewBufferCapacity = 0u;
    }
    native3DPreviewWidth = 0;
    native3DPreviewHeight = 0;
    RayTracingPreview_ShutdownNative3DDirtyRect();
    if (energyBuffer != NULL) {
        free(energyBuffer);
        energyBuffer = NULL;
    }
    if (directEnergyBufferCPU != NULL) {
        free(directEnergyBufferCPU);
        directEnergyBufferCPU = NULL;
    }
    TileGridFree(&tileGrid);
    UniformGridFree(&uniformGrid);
    IrradianceCacheClear(&irradianceCache);
    if (reflectionForwardBuffer) {
        free(reflectionForwardBuffer);
        reflectionForwardBuffer = NULL;
    }
    if (materialTable) {
        free(materialTable);
        materialTable = NULL;
        materialCapacity = 0;
    }
    SurfaceMeshFree(&surfaceMesh);
    TriangleMeshFree(&triangleMesh);

    fluid_frame_free(&g_fluidFrame);
    fluid_manifest_free(&g_fluidManifest);
    g_fluidSpaceValid = false;
}

void SetLightPosition(double x, double y) {
    light.x = x;
    light.y = y;
}

void RenderRayTracingScene(SDL_Renderer* renderer) {
    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    size_t pixelCount = (size_t)WIDTH * (size_t)HEIGHT;

    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RayTracingViewCarrier viewCarrier = RayTracingModeBackend_BuildViewCarrier(&sceneSettings.camera,
                                                                                WIDTH,
                                                                                HEIGHT,
                                                                                &route);
    RayTracingPrimitivePrepPlan primitivePrepPlan =
        RayTracingModeBackend_BuildPrimitivePrepPlan(&route, sceneSettings.objectCount);
    bool useTiles = route.useTiles;
    int tileSize = route.tileSize;
    double gridCellSize = fmax(4.0, (animSettings.tileSize > 0 ? animSettings.tileSize : 16));
    double camera_origin_x = viewCarrier.originX;
    double camera_origin_y = viewCarrier.originY;
    bool native3D = RayTracingModeBackend_IsNative3D(&route);

    // Clamp camera to grid if fluid bounds are known.
    if (g_grid.valid) {
        if (sceneSettings.camera.x < g_grid.min_x) sceneSettings.camera.x = g_grid.min_x;
        if (sceneSettings.camera.x > g_grid.max_x) sceneSettings.camera.x = g_grid.max_x;
        if (sceneSettings.camera.y < g_grid.min_y) sceneSettings.camera.y = g_grid.min_y;
        if (sceneSettings.camera.y > g_grid.max_y) sceneSettings.camera.y = g_grid.max_y;
    }

    // Ensure pixelBuffer is allocated
    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate pixel buffer during render.\n");
            return;
        }
    }
    if (tilePreviewBuffer == NULL) {
        tilePreviewBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!tilePreviewBuffer) {
            printf("ERROR: Failed to allocate tile preview buffer during render.\n");
            return;
        }
    }

    if (!useTiles) {
        if (energyBuffer == NULL) {
            energyBuffer = (float*)malloc(pixelCount * sizeof(float));
            if (!energyBuffer) {
                printf("ERROR: Failed to allocate energy buffer during render.\n");
                return;
            }
        }
        memset(energyBuffer, 0, pixelCount * sizeof(float));
    } else {
        TileGridEnsure(&tileGrid, WIDTH, HEIGHT, tileSize);
        TileGridClear(&tileGrid);
    }
    if (!useTiles) {
        if (directEnergyBufferCPU == NULL) {
            directEnergyBufferCPU = (float*)malloc(pixelCount * sizeof(float));
            if (!directEnergyBufferCPU) {
                printf("ERROR: Failed to allocate direct energy buffer during render.\n");
                return;
            }
        }
        memset(directEnergyBufferCPU, 0, pixelCount * sizeof(float));
    } else {
        // Ensure forward-only buffers are not reused in tiled mode
        if (directEnergyBufferCPU) memset(directEnergyBufferCPU, 0, pixelCount * sizeof(float));
    }
    
    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8)); // Clear buffer
    memset(tilePreviewBuffer, 0, pixelCount * sizeof(Uint8));

    if (native3D) {
        RenderContext* renderContext = getRenderContext();
        RuntimeNative3DRenderStats nativeStats = {0};
        RuntimeNative3DSamplingContext nativeSampling = NextNative3DSamplingContext();
        int blurRadius = 0;
        bool nativeRenderOk = false;
        double normalized_t = AnimationCurrentNormalizedT();
        int renderScale = RuntimeNative3DClampRenderScale(animSettings.renderScale3D);
        int hostWidth = WIDTH;
        int hostHeight = HEIGHT;
        int renderWidth = WIDTH;
        int renderHeight = HEIGHT;
        size_t renderPixelCount = 0u;
        size_t hostPixelCount = pixelCount;
        size_t nativePreviewByteCount =
            hostPixelCount * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;

        if (!RuntimeNative3DResolveHostDimensions(WIDTH,
                                                  HEIGHT,
                                                  renderContext ? renderContext->width : WIDTH,
                                                  renderContext ? renderContext->height : HEIGHT,
                                                  renderScale,
                                                  &hostWidth,
                                                  &hostHeight)) {
            memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
            return;
        }
        hostPixelCount = (size_t)hostWidth * (size_t)hostHeight;
        nativePreviewByteCount = hostPixelCount * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        if (!RuntimeNative3DResolveScaledDimensions(hostWidth,
                                                    hostHeight,
                                                    renderScale,
                                                    &renderWidth,
                                                    &renderHeight)) {
            memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
            return;
        }
        renderPixelCount = (size_t)renderWidth * (size_t)renderHeight;
        if (!EnsureNative3DRenderBuffer(renderPixelCount) ||
            !EnsureNative3DPreviewBuffer(hostPixelCount)) {
            printf("ERROR: Failed to allocate native 3D render buffer during render.\n");
            memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
            return;
        }
        native3DPreviewWidth = hostWidth;
        native3DPreviewHeight = hostHeight;
        RuntimeNative3DFillPixelBufferEnvironment(native3DRenderBuffer, renderPixelCount);
        RuntimeNative3DFillPixelBufferEnvironment(native3DPreviewBuffer,
                                                 nativePreviewByteCount /
                                                     (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);

        ts_session_start_timer(timer_hud_session(), "Buffer Calc");
        if (useTiles && tileGrid.tiles && tileGrid.count > 0) {
            TileGridEnsure(&tileGrid, renderWidth, renderHeight, tileSize);
            TileGridClear(&tileGrid);
            nativeRenderOk = RayTracing2PreviewPresent_RenderNative3DTilesPreview(
                renderer,
                native3DPreviewBuffer,
                hostWidth,
                hostHeight,
                native3DRenderBuffer,
                renderWidth,
                renderHeight,
                &tileGrid,
                route.integratorMode3D,
                normalized_t,
                light.x,
                light.y,
                &nativeSampling,
                ResolveNative3DTemporalFrames(route.integratorMode3D),
                animSettings.disneyDenoiseEnabled,
                route.tilePreviewEnabled,
                &nativeStats);
        } else {
            nativeRenderOk = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
                native3DRenderBuffer,
                route.integratorMode3D,
                renderWidth,
                renderHeight,
                normalized_t,
                light.x,
                light.y,
                &nativeSampling,
                ResolveNative3DTemporalFrames(route.integratorMode3D),
                &nativeStats);
        }
        ts_session_stop_timer(timer_hud_session(), "Buffer Calc");
        if (!nativeRenderOk) {
            memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
            RuntimeNative3DFillPixelBufferEnvironment(
                native3DPreviewBuffer,
                nativePreviewByteCount / (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
        } else {
            LogNative3DRenderStatsIfNeeded(route.integratorMode3D, &nativeStats);
        }

        if (animSettings.blurMode == 1) blurRadius = 1;
        else if (animSettings.blurMode == 2) blurRadius = 2;
        else if (animSettings.blurMode == 3) blurRadius = 3;

        ts_session_start_timer(timer_hud_session(), "Buffer Present");
        if (nativeRenderOk) {
            if (blurRadius > 0) {
                RayTracingPreview_ApplySeparableBlurABGR(native3DRenderBuffer,
                                                         renderWidth,
                                                         renderHeight,
                                                         blurRadius);
            }
            RuntimeNative3DUpscaleNearestABGR(native3DRenderBuffer,
                                              renderWidth,
                                              renderHeight,
                                              native3DPreviewBuffer,
                                              hostWidth,
                                              hostHeight);
        }
#if USE_VULKAN
        RayTracing2PreviewPresent_DrawABGRBufferToRect(renderer,
                                                       native3DPreviewBuffer,
                                                       hostWidth,
                                                       hostHeight,
                                                       (SDL_Rect){0, 0, WIDTH, HEIGHT});
#else
        for (int y = 0; y < hostHeight; y++) {
            for (int x = 0; x < hostWidth; x++) {
                size_t idx =
                    ((size_t)y * (size_t)hostWidth + (size_t)x) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
                Uint8 b = native3DPreviewBuffer[idx];
                Uint8 g = native3DPreviewBuffer[idx + 1u];
                Uint8 r = native3DPreviewBuffer[idx + 2u];
                if (r > 0 || g > 0 || b > 0) {
                    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                    SDL_RenderDrawPoint(renderer, x, y);
                }
            }
        }
#endif
        ts_session_stop_timer(timer_hud_session(), "Buffer Present");
        return;
    }

    uint64_t frameSeed = runtime_time_now_ns();
    int materialCount = BuildMaterialTable();

    bool haveCache = false;
    if (route.buildIrradianceCache) {
        haveCache = IrradianceCacheEnsure(&irradianceCache,
                                          sceneSettings.objectCount,
                                          32);
    }

    bool meshReady = false;
    if (primitivePrepPlan.enableSurfaceMeshPrep || primitivePrepPlan.enableTriangleMeshPrep) {
        meshReady = SurfaceBuildMeshes(&surfaceMesh,
                                       &triangleMesh,
                                       sceneSettings.sceneObjects,
                                       sceneSettings.objectCount,
                                       8.0);
    }

    if (primitivePrepPlan.enableUniformGrid2D && primitivePrepPlan.enableRay2DIntersections) {
        UniformGridBuild(&uniformGrid,
                         sceneSettings.sceneObjects,
                         sceneSettings.objectCount,
                         (meshReady && primitivePrepPlan.enableTriangleMeshPrep) ? &triangleMesh : NULL,
                         gridCellSize);
    } else {
        UniformGridClear(&uniformGrid);
    }

    IntegratorContext context = {
        .pixelBuffer = pixelBuffer,
        .energyBuffer = useTiles ? NULL : energyBuffer,
        .directEnergyBuffer = useTiles ? NULL : directEnergyBufferCPU,
        .width = WIDTH,
        .height = HEIGHT,
        .objects = sceneSettings.sceneObjects,
        .objectCount = sceneSettings.objectCount,
        .tileGrid = useTiles ? &tileGrid : NULL,
        .useTiles = useTiles,
        .frameSeed = frameSeed,
        .uniformGrid = (primitivePrepPlan.enableRay2DIntersections &&
                        (uniformGrid.objectCells || uniformGrid.triangleCells))
                           ? &uniformGrid
                           : NULL,
        .integratorMode = route.integratorMode,
        .cache = (haveCache ? &irradianceCache : NULL),
        .materials = materialTable,
        .materialCount = materialCount,
        .mesh = (meshReady && primitivePrepPlan.enableSurfaceMeshPrep) ? &surfaceMesh : NULL,
        .triangleMesh = (meshReady && primitivePrepPlan.enableTriangleMeshPrep) ? &triangleMesh : NULL
    };

    LightSource activeLight = {
        .x = light.x,
        .y = light.y,
        .radius = light.r
    };

    bool cacheReady = false;
    if (route.buildIrradianceCache && haveCache) {
        ts_session_start_timer(timer_hud_session(), "Irradiance Cache");
        cacheReady = BuildReflectionCache(&context, &activeLight);
        ts_session_stop_timer(timer_hud_session(), "Irradiance Cache");
        if (!cacheReady) {
            context.cache = NULL;
        }
    } else {
        // Do not reuse stale caches when not in camera-path mode
        context.cache = NULL;
    }

    ts_session_start_timer(timer_hud_session(), "Buffer Calc");
    switch (route.integratorMode) {
        case 0: // forward
            ForwardLightIntegratorRender(&context, &activeLight);
            break;
        case 1: { // hybrid camera path (new split)
            CameraIntegratorSettings settings = {
                .directIntensityScale = animSettings.lightIntensity,
                .indirectVariance = animSettings.cacheVarianceCutoff,
                .indirectHaloRadius = animSettings.cacheHaloRadius,
                .blurEnabled = (animSettings.blurMode != 0),
                .brightnessBoost = 1.0
            };
            if (route.tilePreviewEnabled) {
                RayTracing2PreviewPresent_RenderHybridTilesPreview(
                    renderer,
                    &context,
                    &activeLight,
                    &settings,
                    camera_origin_x,
                    camera_origin_y,
                    tilePreviewBuffer ? tilePreviewBuffer : context.pixelBuffer);
            } else {
                CameraPathIntegratorRenderFromContext(&context,
                                                      &activeLight,
                                                      &settings,
                                                      camera_origin_x,
                                                      camera_origin_y);
            }
            break;
        }
        case 2: // direct-only
            DirectLightIntegratorRender(&context, &activeLight);
            break;
        default:
            ForwardLightIntegratorRender(&context, &activeLight);
            break;
    }
    ts_session_stop_timer(timer_hud_session(), "Buffer Calc");

    int blurRadius = 0;
    if (animSettings.blurMode == 1) blurRadius = 1;
    else if (animSettings.blurMode == 2) blurRadius = 2;
    else if (animSettings.blurMode == 3) blurRadius = 3;
    ts_session_start_timer(timer_hud_session(), "Buffer Present");
    if (route.tilePreviewEnabled) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect bg = {0, 0, WIDTH, HEIGHT};
        SDL_RenderFillRect(renderer, &bg);
    }
    if (blurRadius > 0) {
        RayTracingPreview_ApplySeparableBlur(pixelBuffer, WIDTH, HEIGHT, blurRadius);
    }

#if USE_VULKAN
    RayTracing2PreviewPresent_DrawLuminanceBuffer(renderer,
                                                  pixelBuffer,
                                                  WIDTH,
                                                  HEIGHT);
#else
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Uint8 brightness = pixelBuffer[y * WIDTH + x];
            if (brightness > 0) {
                SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
#endif
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SpaceModeViewContext view_ctx = RayTracingModeBackend_BuildViewContext(&sceneSettings.camera,
                                                                           WIDTH,
                                                                           HEIGHT,
                                                                           &route);
    CameraPoint lightScreen = SpaceModeAdapter_WorldToScreen(&view_ctx, light.x, light.y);
    int lightRadius = (int)lround(light.r * sceneSettings.camera.zoom);
    RenderCircle(renderer, (int)lround(lightScreen.x), (int)lround(lightScreen.y), lightRadius, true);


    // ✅ Draw objects using the new method
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        if (!SceneObjectParticipatesInRender(&sceneSettings.sceneObjects[i])) continue;
        int brightness = CalculateObjectBrightness(&sceneSettings.sceneObjects[i], light.x, light.y);
        SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
        RenderSceneObject(renderer, &sceneSettings.sceneObjects[i], true);
    }

    // Fluid overlay (density) drawn after objects for visibility.
    if (g_fluidOverlayEnabled && g_fluidManifest.count > 0) {
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
            fluid_overlay_draw(renderer, &g_fluidFrame, &sceneSettings.camera, WIDTH, HEIGHT);
        }
    }

    ts_session_stop_timer(timer_hud_session(), "Buffer Present");
}


void ProcessRayTracingEvent(SDL_Event* event) {
    if (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN) {
        RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
        RayTracingViewCarrier viewCarrier = RayTracingModeBackend_BuildViewCarrier(&sceneSettings.camera,
                                                                                   sceneSettings.windowWidth,
                                                                                   sceneSettings.windowHeight,
                                                                                   &route);
        CameraPoint world = SpaceModeAdapter_ScreenToWorld(&viewCarrier.viewContext,
                                                           event->motion.x,
                                                           event->motion.y);
        light.x = world.x;
        light.y = world.y;
    }
    else if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_b) {  // Press "B" to switch blur mode
            animSettings.blurMode = (animSettings.blurMode == 2) ? 0 : animSettings.blurMode + 1;
            printf("Blur Mode: %s\n", (animSettings.blurMode == 0) ? "None" :
                                        (animSettings.blurMode == 1) ? "Light Blur" :
                                        "Heavy Blur");
    }
}


void GetCurrentLightPosition(double* x, double* y) {
    *x = light.x;
    *y = light.y;
}

static int BuildMaterialTable(void) {
    int count = sceneSettings.objectCount;
    if (count <= 0) {
        if (materialTable) {
            free(materialTable);
            materialTable = NULL;
        }
        materialCapacity = 0;
        return 0;
    }

    if (!materialTable || materialCapacity < count) {
        MaterialBSDF* newBuffer = (MaterialBSDF*)realloc(materialTable, (size_t)count * sizeof(MaterialBSDF));
        if (!newBuffer) {
            fprintf(stderr, "ERROR: Failed to allocate material table.\n");
            free(materialTable);
            materialTable = NULL;
            materialCapacity = 0;
            return 0;
        }
        materialTable = newBuffer;
        materialCapacity = count;
    }

    for (int i = 0; i < count; i++) {
        MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[i], &materialTable[i]);
        ApplyMaterialOverrides(&materialTable[i]);
    }
    return count;
}
static bool BuildReflectionCache(const IntegratorContext* ctx,
                                 const LightSource* light) {
    if (!ctx || !ctx->cache) return false;
    int width = ctx->width;
    int height = ctx->height;
    size_t pixelCount = (size_t)width * (size_t)height;
    if (!reflectionForwardBuffer) {
        reflectionForwardBuffer = (float*)malloc(pixelCount * sizeof(float));
        if (!reflectionForwardBuffer) {
            return false;
        }
    }

    int savedRays = sceneSettings.rays;
    int probeRays = savedRays / 6;
    if (probeRays < 128) probeRays = 128;
    sceneSettings.rays = probeRays;

    IntegratorContext probeCtx = *ctx;
    probeCtx.pixelBuffer = NULL;
    probeCtx.energyBuffer = reflectionForwardBuffer;
    probeCtx.useTiles = false;
    probeCtx.tileGrid = NULL;
    memset(reflectionForwardBuffer, 0, pixelCount * sizeof(float));
    ForwardLightIntegratorRender(&probeCtx, light);
    sceneSettings.rays = savedRays;

    float maxEnergy = 0.0f;
    for (size_t i = 0; i < pixelCount; i++) {
        if (reflectionForwardBuffer[i] > maxEnergy) {
            maxEnergy = reflectionForwardBuffer[i];
        }
    }
    if (maxEnergy <= 0.0f) {
        maxEnergy = 1.0f;
    }

    // Two-pass cache: first direct-only to seed, then include indirect reflections.
    bool ok = IrradianceCacheFill(ctx->cache,
                                  ctx->objects,
                                  ctx->objectCount,
                                  light,
                                  ctx->uniformGrid,
                                  reflectionForwardBuffer,
                                  width,
                                  height,
                                  (double)maxEnergy,
                                  NULL,
                                  0,
                                  NULL,
                                  false);
    if (!ok) return false;
    return IrradianceCacheFill(ctx->cache,
                               ctx->objects,
                               ctx->objectCount,
                               light,
                               ctx->uniformGrid,
                               reflectionForwardBuffer,
                               width,
                               height,
                               (double)maxEnergy,
                               NULL,
                               0,
                               NULL,
                               true);
}
