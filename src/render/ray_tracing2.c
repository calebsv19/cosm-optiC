#include "render/ray_tracing2.h"
#include "config/config_manager.h"  // Include animation.h to access `sceneObjects[]` and `objectCount`
#include "render/render_helper.h"
#include "render/fast_rng.h"
#include "render/uniform_grid.h"
#include "render/ray_types.h"
#include "render/integrator_common.h"
#include "render/material_bsdf.h"
#include "render/surface_mesh.h"
#include "render/camera_path_integrator.h"
#include "render/forward_light_integrator.h"
#include "render/irradiance_cache.h"
#include "render/timer_hud_api.h"
#include "engine/Render/render_pipeline.h"
#include "editor/scene_editor.h"
#include "app/animation.h"
#include "camera/camera.h"
#include <stdio.h>   
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <float.h>

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif


// Global light source.
static Circle light = {100, 100, 10};  // Default position (updated dynamically)
Uint8* pixelBuffer = NULL;  // Global but uninitialized
float* energyBuffer = NULL;
float* directEnergyBufferCPU = NULL;
static TileGrid tileGrid = {0};
static UniformGrid uniformGrid = {0};
static IrradianceCache irradianceCache = {0};
static MaterialBSDF* materialTable = NULL;
static int materialCapacity = 0;
static SurfaceMesh surfaceMesh = {0};

static float* reflectionForwardBuffer = NULL;
static bool BuildReflectionCache(const IntegratorContext* ctx,
                                 const LightSource* light);
static int BuildMaterialTable(void);

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

    if (energyBuffer == NULL) {
        energyBuffer = (float*)malloc(pixelCount * sizeof(float));
        if (!energyBuffer) {
            printf("ERROR: Failed to allocate memory for energy buffer!\n");
            exit(1);
        }
    }

    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8));
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
}

void CleanupRayTracing(void) {
    if (pixelBuffer != NULL) {
        free(pixelBuffer);
        pixelBuffer = NULL;
    }
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
}

void SetLightPosition(double x, double y) {
    light.x = x;
    light.y = y;
}

static void BuildGaussianKernel(float* kernel, int radius) {
    float sigma = (float)radius * 0.5f + 0.5f;
    float sum = 0.0f;
    for (int i = -radius; i <= radius; i++) {
        float value = expf(-(i * i) / (2.0f * sigma * sigma));
        kernel[i + radius] = value;
        sum += value;
    }
    if (sum > 0.0f) {
        for (int i = 0; i < (2 * radius + 1); i++) {
            kernel[i] /= sum;
        }
    }
}

static void ApplySeparableBlur(Uint8* buffer, int width, int height, int radius) {
    if (radius <= 0 || !buffer) return;
    int kernelSize = radius * 2 + 1;
    float* kernel = (float*)malloc((size_t)kernelSize * sizeof(float));
    if (!kernel) return;
    BuildGaussianKernel(kernel, radius);

    size_t total = (size_t)width * (size_t)height;
    float* temp = (float*)malloc(total * sizeof(float));
    float* output = (float*)malloc(total * sizeof(float));
    if (!temp || !output) {
        free(kernel);
        free(temp);
        free(output);
        return;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            for (int k = -radius; k <= radius; k++) {
                int sx = x + k;
                if (sx < 0) sx = 0;
                if (sx >= width) sx = width - 1;
                accum += kernel[k + radius] * buffer[y * width + sx];
            }
            temp[y * width + x] = accum;
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            for (int k = -radius; k <= radius; k++) {
                int sy = y + k;
                if (sy < 0) sy = 0;
                if (sy >= height) sy = height - 1;
                accum += kernel[k + radius] * temp[sy * width + x];
            }
            output[y * width + x] = accum;
        }
    }

    for (size_t i = 0; i < total; i++) {
        buffer[i] = (Uint8)Clamp(output[i], 0, 255);
    }

    free(kernel);
    free(temp);
    free(output);
}

void RenderRayTracingScene(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    size_t pixelCount = (size_t)WIDTH * (size_t)HEIGHT;

    bool useTiles = animSettings.useTiledRenderer;
    int tileSize = useTiles ? ClampTileSize(animSettings.tileSize) : 0;
    double gridCellSize = fmax(4.0, (animSettings.tileSize > 0 ? animSettings.tileSize : 16));
    UniformGridBuild(&uniformGrid, sceneSettings.sceneObjects, sceneSettings.objectCount, gridCellSize);

    // Ensure pixelBuffer is allocated
    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate pixel buffer during render.\n");
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
    }
    
    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8)); // Clear buffer

    uint64_t frameSeed = (uint64_t)SDL_GetPerformanceCounter();
    int materialCount = BuildMaterialTable();

    bool haveCache = false;
    if (animSettings.integratorMode == 1) {
        haveCache = IrradianceCacheEnsure(&irradianceCache,
                                          sceneSettings.objectCount,
                                          64);
    }

    bool meshReady = SurfaceMeshBuild(&surfaceMesh,
                                      sceneSettings.sceneObjects,
                                      sceneSettings.objectCount,
                                      8.0);

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
        .uniformGrid = (uniformGrid.cells ? &uniformGrid : NULL),
        .integratorMode = animSettings.integratorMode,
        .cache = (haveCache ? &irradianceCache : NULL),
        .materials = materialTable,
        .materialCount = materialCount,
        .mesh = (meshReady ? &surfaceMesh : NULL)
    };

    LightSource activeLight = {
        .x = light.x,
        .y = light.y,
        .radius = light.r
    };

    bool cacheReady = false;
    if (animSettings.integratorMode == 1 && haveCache) {
        ts_start_timer("Irradiance Cache");
        cacheReady = BuildReflectionCache(&context, &activeLight);
        ts_stop_timer("Irradiance Cache");
        if (!cacheReady) {
            context.cache = NULL;
        }
    }

    ts_start_timer("Buffer Calc");
    if (animSettings.integratorMode == 0) {
        ForwardLightIntegratorRender(&context, &activeLight);
    } else {
        CameraPathIntegratorRender(&context, &activeLight);
    }
    ts_stop_timer("Buffer Calc");

    int blurRadius = 0;
    if (animSettings.blurMode == 1) blurRadius = 1;
    else if (animSettings.blurMode == 2) blurRadius = 2;
    else if (animSettings.blurMode == 3) blurRadius = 3;
    ts_start_timer("Buffer Present");
    if (blurRadius > 0) {
        ApplySeparableBlur(pixelBuffer, WIDTH, HEIGHT, blurRadius);
    }

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Uint8 brightness = pixelBuffer[y * WIDTH + x];
            if (brightness > 0) {
                SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    CameraPoint lightScreen = CameraWorldToScreen(&sceneSettings.camera,
                                                  light.x,
                                                  light.y,
                                                  WIDTH,
                                                  HEIGHT);
    int lightRadius = (int)lround(light.r * sceneSettings.camera.zoom);
    RenderCircle(renderer, (int)lround(lightScreen.x), (int)lround(lightScreen.y), lightRadius, true);


    // ✅ Draw objects using the new method
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        int brightness = CalculateObjectBrightness(&sceneSettings.sceneObjects[i], light.x, light.y);
	SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
        RenderSceneObject(renderer, &sceneSettings.sceneObjects[i], true);
    }

    ts_stop_timer("Buffer Present");
}


void ProcessRayTracingEvent(SDL_Event* event) {
    if (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN) {
        CameraPoint world = CameraScreenToWorld(&sceneSettings.camera,
                                                event->motion.x,
                                                event->motion.y,
                                                sceneSettings.windowWidth,
                                                sceneSettings.windowHeight);
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

    return IrradianceCacheFill(ctx->cache,
                               ctx->objects,
                               ctx->objectCount,
                               light,
                               ctx->uniformGrid,
                               reflectionForwardBuffer,
                               width,
                               height,
                               (double)maxEnergy);
}
