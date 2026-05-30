#include "render/ray_tracing2.h"
#include "config/config_manager.h"  // Include animation.h to access `sceneObjects[]` and `objectCount`
#include "render/render_helper.h"
#include "render/fast_rng.h"
#include "render/uniform_grid.h"
#include "render/ray_types.h"
#include "render/integrator_common.h"
#include "render/material_bsdf.h"
#include "render/ray_tracing2_preview.h"
#include "render/pipeline/ray_tracing2_buffers.h"
#include "render/pipeline/ray_tracing2_internal.h"
#include "render/pipeline/ray_tracing2_native3d_overlay.h"
#include "render/pipeline/ray_tracing2_preview_present.h"
#include "render/surface_mesh.h"
#include "render/integrators/hybrid/camera_path_integrator.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/integrators/hybrid/integrator_direct.h"
#include "render/integrators/hybrid/integrator_indirect.h"
#include "render/integrators/forward_light_integrator.h"
#include "render/integrators/direct_light_integrator.h"
#include "render/timer_hud_api.h"
#include "render/fluid_overlay.h"
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
#include "render/runtime_native_3d_preview_reconstruction.h"
#include "render/runtime_native_3d_resolution.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d_samples.h"
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
    if (stats->temporalMeasuredTileJobs > 0) {
        printf("[native3d] tile_metrics jobs=%d avg_ms=%.3f max_tile_ms=%.3f max_subpass_ms=%.3f splits=%d children=%d slow_tile=(%d,%d %dx%d)\n",
               stats->temporalMeasuredTileJobs,
               stats->temporalAverageTileMs,
               stats->temporalMaxTileMs,
               stats->temporalMaxTileSubpassMs,
               stats->temporalAdaptiveSplitParentCount,
               stats->temporalAdaptiveChildTileCount,
               stats->temporalSlowTileOriginX,
               stats->temporalSlowTileOriginY,
               stats->temporalSlowTileWidth,
               stats->temporalSlowTileHeight);
    }
}

bool ExportCurrentNative3DFrameBMP(const char* filename) {
    int exportWidth = native3DPreviewWidth > 0 ? native3DPreviewWidth : sceneSettings.windowWidth;
    int exportHeight = native3DPreviewHeight > 0 ? native3DPreviewHeight : sceneSettings.windowHeight;
    return RayTracing2Native3DOverlay_ExportFrameBMP(filename,
                                                     exportWidth,
                                                     exportHeight,
                                                     native3DPreviewBuffer,
                                                     pixelBuffer);
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

    if (!RayTracing2BuffersEnsureFrameBuffers(&pixelBuffer,
                                              &tilePreviewBuffer,
                                              &energyBuffer,
                                              pixelCount)) {
        printf("ERROR: Failed to allocate core frame buffers!\n");
        exit(1);
    }
    RayTracing2BuffersClearFrameBuffers(pixelBuffer,
                                        tilePreviewBuffer,
                                        energyBuffer,
                                        pixelCount);

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

    (void)RayTracing2Fluid_InitializeScene();
}

void CleanupRayTracing(void) {
    RayTracing2BuffersResetFrameBuffers(&pixelBuffer,
                                        &tilePreviewBuffer,
                                        &energyBuffer,
                                        &directEnergyBufferCPU);
    RayTracing2BuffersResetNative3D(&native3DRenderBuffer,
                                    &native3DRenderBufferCapacity,
                                    &native3DPreviewBuffer,
                                    &native3DPreviewBufferCapacity,
                                    &native3DPreviewWidth,
                                    &native3DPreviewHeight);
    RayTracingPreview_ShutdownNative3DDirtyRect();
    TileGridFree(&tileGrid);
    UniformGridFree(&uniformGrid);
    IrradianceCacheClear(&irradianceCache);
    RayTracing2Materials_Cleanup(&materialTable, &materialCapacity, &reflectionForwardBuffer);
    SurfaceMeshFree(&surfaceMesh);
    TriangleMeshFree(&triangleMesh);
    RayTracing2Fluid_CleanupScene();
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
    RayTracing2Fluid_ClampCameraToGrid();

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
        int nativeTileSize = 0;
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
        nativeTileSize = RuntimeNative3DTileSchedulerResolveTileSizeForScale(animSettings.tileSize,
                                                                             renderScale);
        if (!RayTracing2BuffersEnsureNative3DRenderBuffer(&native3DRenderBuffer,
                                                          &native3DRenderBufferCapacity,
                                                          renderPixelCount) ||
            !RayTracing2BuffersEnsureNative3DPreviewBuffer(&native3DPreviewBuffer,
                                                           &native3DPreviewBufferCapacity,
                                                           hostPixelCount)) {
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
            TileGridEnsure(&tileGrid, renderWidth, renderHeight, nativeTileSize);
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
            if (animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_OFF) {
                RuntimeNative3DUpscaleNearestABGR(native3DRenderBuffer,
                                                  renderWidth,
                                                  renderHeight,
                                                  native3DPreviewBuffer,
                                                  hostWidth,
                                                  hostHeight);
            } else {
                (void)RuntimeNative3DPreviewReconstructABGRWithMode(
                    native3DRenderBuffer,
                    renderWidth,
                    renderHeight,
                    native3DPreviewBuffer,
                    hostWidth,
                    hostHeight,
                    (Runtime3DUpscaleMode)animSettings.upscaleMode3D);
            }
        }
#if USE_VULKAN
        RayTracing2PreviewPresent_DrawABGRBufferToRectFiltered(
            renderer,
            native3DPreviewBuffer,
            hostWidth,
            hostHeight,
            (SDL_Rect){0, 0, WIDTH, HEIGHT},
            animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_BILINEAR);
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
    int materialCount = RayTracing2Materials_BuildTable(&materialTable, &materialCapacity);

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
        cacheReady = RayTracing2Materials_BuildReflectionCache(&context,
                                                               &activeLight,
                                                               &reflectionForwardBuffer);
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
    RayTracing2Fluid_RenderOverlay(renderer, WIDTH, HEIGHT);

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
    else if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_u) {
            if (animSettings.upscaleMode3D < RUNTIME_3D_UPSCALE_MODE_MIN ||
                animSettings.upscaleMode3D > RUNTIME_3D_UPSCALE_MODE_MAX) {
                animSettings.upscaleMode3D = RUNTIME_3D_UPSCALE_MODE_DEFAULT;
            } else {
                animSettings.upscaleMode3D =
                    (animSettings.upscaleMode3D + 1) % (RUNTIME_3D_UPSCALE_MODE_MAX + 1);
            }
            printf("3D Upscale Mode: %s\n",
                   RayTracing2Native3DOverlay_ResolveUpscaleModeLabel(animSettings.upscaleMode3D));
    }
}


void GetCurrentLightPosition(double* x, double* y) {
    *x = light.x;
    *y = light.y;
}
