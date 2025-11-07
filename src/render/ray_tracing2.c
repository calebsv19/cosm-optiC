#include "render/ray_tracing2.h"
#include "config/config_manager.h"  // Include animation.h to access `sceneObjects[]` and `objectCount`
#include "render/render_helper.h"
#include "editor/scene_editor.h"
#include "app/animation.h"
#include "camera/camera.h"
#include <stdio.h>   
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

typedef struct {
    Uint8* pixelBuffer;
    float* energyBuffer;
    int width;
    int height;
    SceneObject* objects;
    int objectCount;
} TraceContext;

typedef struct {
    double sourceX, sourceY;
    int rayStart, rayEnd;
    const TraceContext* ctx;
} ThreadData;


// Global light source.
static Circle light = {100, 100, 10};  // Default position (updated dynamically)
const int TOTAL_RAYS = 1800;
Uint8* pixelBuffer = NULL;  // Global but uninitialized
float* energyBuffer = NULL;

static bool WorldToPixel(double worldX, double worldY,
                         int width, int height, int* pixelIndex,
                         int* screenX, int* screenY) {
    CameraPoint screen = CameraWorldToScreen(&sceneSettings.camera,
                                             worldX,
                                             worldY,
                                             width,
                                             height);
    int sx = (int)lround(screen.x);
    int sy = (int)lround(screen.y);
    if (screenX) *screenX = sx;
    if (screenY) *screenY = sy;
    if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
        return false;
    }
    if (pixelIndex) {
        *pixelIndex = sy * width + sx;
    }
    return true;
}

#define MAX_BOUNCES 3
#define MIN_ENERGY 0.003
#define ENERGY_BOOST 4.5

static double Clamp(double value, double minValue, double maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static double Clamp01(double value) {
    return Clamp(value, 0.0, 1.0);
}

static void Normalize(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 0.0001) {
        *x /= len;
        *y /= len;
    }
}

static double Noise2D(double x, double y) {
    double s = sin(x * 12.9898 + y * 78.233);
    double frac = s - floor(s);
    return frac;
}

static void DepositEnergy(const TraceContext* ctx, double worldX, double worldY, double energy) {
    if (energy <= 0.0 || ctx->energyBuffer == NULL) return;
    int pixelIndex;
    if (!WorldToPixel(worldX, worldY, ctx->width, ctx->height, &pixelIndex, NULL, NULL)) {
        return;
    }
    ctx->energyBuffer[pixelIndex] += (float)energy;
}

static void ApplyEnergyDiffusion(TraceContext* ctx, int radius, double strength) {
    if (!ctx->energyBuffer || radius <= 0 || strength <= 0.0) return;
    int width = ctx->width;
    int height = ctx->height;
    size_t total = (size_t)width * (size_t)height;
    float* temp = (float*)malloc(total * sizeof(float));
    if (!temp) return;

    int clampedRadius = radius > 20 ? 20 : radius;
    if (clampedRadius < 1) {
        free(temp);
        return;
    }

    float sigma = (float)clampedRadius * 0.5f + 0.5f;
    float twoSigmaSq = 2.0f * sigma * sigma;
    float blend = (float)Clamp01(strength);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            float weightSum = 0.0f;
            for (int dy = -clampedRadius; dy <= clampedRadius; dy++) {
                int sy = y + dy;
                if (sy < 0 || sy >= height) continue;
                for (int dx = -clampedRadius; dx <= clampedRadius; dx++) {
                    int sx = x + dx;
                    if (sx < 0 || sx >= width) continue;
                    float dist2 = (float)(dx * dx + dy * dy);
                    float weight = expf(-dist2 / twoSigmaSq);
                    accum += ctx->energyBuffer[sy * width + sx] * weight;
                    weightSum += weight;
                }
            }
            float blurred = (weightSum > 0.0f) ? (accum / weightSum) : ctx->energyBuffer[y * width + x];
            float original = ctx->energyBuffer[y * width + x];
            temp[y * width + x] = (1.0f - blend) * original + blend * blurred;
        }
    }

    memcpy(ctx->energyBuffer, temp, total * sizeof(float));
    free(temp);
}

static void ConvertEnergyToPixels(TraceContext* ctx) {
    if (!ctx->pixelBuffer || !ctx->energyBuffer) return;
    int total = ctx->width * ctx->height;
    float maxEnergy = 0.0f;
    for (int i = 0; i < total; i++) {
        float val = ctx->energyBuffer[i];
        if (val > maxEnergy) {
            maxEnergy = val;
        }
    }
    if (maxEnergy <= 0.0f) {
        memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
        return;
    }
    float invMax = 1.0f / maxEnergy;
    for (int i = 0; i < total; i++) {
        float normalized = ctx->energyBuffer[i] * invMax * ENERGY_BOOST;
        float tone = powf(Clamp01(normalized), 0.55f);
        ctx->pixelBuffer[i] = (Uint8)Clamp(tone * 255.0f, 0, 255);
    }
}

static void ReflectDirection(double dx, double dy, double nx, double ny, double* rx, double* ry) {
    double dot = dx * nx + dy * ny;
    *rx = dx - 2.0 * dot * nx;
    *ry = dy - 2.0 * dot * ny;
    Normalize(rx, ry);
}

static void ApplyRoughness(double* dx, double* dy, double roughness, double jitter) {
    if (roughness <= 0.0) return;
    double angle = atan2(*dy, *dx);
    double spread = roughness * 0.5 * M_PI;
    double offset = (jitter - floor(jitter)) - 0.5;
    angle += offset * spread;
    *dx = cos(angle);
    *dy = sin(angle);
}

static bool ComputeSurfaceNormal(const SceneObject* obj, double px, double py, double* nx, double* ny) {
    if (strcmp(obj->type, "circle") == 0) {
        *nx = px - obj->x;
        *ny = py - obj->y;
        Normalize(nx, ny);
        return true;
    }

    if (obj->numPoints < 2) {
        return false;
    }

    double minDist = 1e9;
    double bestNx = 0.0, bestNy = 0.0;
    for (int i = 0; i < obj->numPoints; i++) {
        int next = (i + 1) % obj->numPoints;
        double x1 = obj->shapePoints[i][0] + obj->x;
        double y1 = obj->shapePoints[i][1] + obj->y;
        double x2 = obj->shapePoints[next][0] + obj->x;
        double y2 = obj->shapePoints[next][1] + obj->y;

        double edgeX = x2 - x1;
        double edgeY = y2 - y1;
        double edgeLen = edgeX * edgeX + edgeY * edgeY;
        if (edgeLen < 1e-6) continue;

        double t = ((px - x1) * edgeX + (py - y1) * edgeY) / edgeLen;
        t = Clamp01(t);
        double closestX = x1 + edgeX * t;
        double closestY = y1 + edgeY * t;
        double dx = px - closestX;
        double dy = py - closestY;
        double dist = dx * dx + dy * dy;

        if (dist < minDist) {
            minDist = dist;
            bestNx = dy;
            bestNy = -dx;
        }
    }

    if (minDist < 1e8) {
        double centerDX = px - obj->x;
        double centerDY = py - obj->y;
        double dot = bestNx * centerDX + bestNy * centerDY;
        if (dot < 0) {
            bestNx = -bestNx;
            bestNy = -bestNy;
        }
        Normalize(&bestNx, &bestNy);
        *nx = bestNx;
        *ny = bestNy;
        return true;
    }
    return false;
}

static double SampleTexture(const SceneObject* obj, double px, double py) {
    (void)px;
    (void)py;
    double r = (double)((obj->color >> 16) & 0xFF);
    double g = (double)((obj->color >> 8) & 0xFF);
    double b = (double)(obj->color & 0xFF);
    double colorLuma = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
    double base = Clamp01(colorLuma);
    if (obj->opacity < 1.0) {
        base *= obj->opacity;
    }

    switch (obj->textureId) {
        case 1: {
            int checker = (((int)floor(px / 25.0) + (int)floor(py / 25.0)) & 1);
            return base * (checker ? 0.8 : 0.4);
        }
        case 2: {
            double stripes = fabs(sin(px * 0.1));
            return base * (0.5 + 0.5 * stripes);
        }
        case 3: {
            double ring = fmod(sqrt((px - obj->x) * (px - obj->x) + (py - obj->y) * (py - obj->y)), 30.0);
            return base * (ring < 15.0 ? 0.9 : 0.3);
        }
        default:
            return base;
    }
}

static void TraceRayRecursive(const TraceContext* ctx,
                              double originX,
                              double originY,
                              double dirX,
                              double dirY,
                              int depth,
                              double energy) {
    if (depth > MAX_BOUNCES || energy < MIN_ENERGY) {
        return;
    }

    double x = originX;
    double y = originY;
    double step = 1.5;
    double decay = 0.9985;
    int maxSteps = ctx->width + ctx->height;

    for (int i = 0; i < maxSteps; i++) {
        x += dirX * step;
        y += dirY * step;

        if (!WorldToPixel(x, y, ctx->width, ctx->height, NULL, NULL, NULL)) {
            break;
        }

        DepositEnergy(ctx, x, y, energy * 0.8);

        SceneObject* hitObj = NULL;
        for (int j = 0; j < ctx->objectCount; j++) {
            if (IsInsideObject((int)x, (int)y, &ctx->objects[j])) {
                hitObj = &ctx->objects[j];
                break;
            }
        }

        if (hitObj) {
            double nx = 0.0, ny = 0.0;
            if (!ComputeSurfaceNormal(hitObj, x, y, &nx, &ny)) {
                nx = -dirX;
                ny = -dirY;
            }

            double surfaceSample = SampleTexture(hitObj, x, y);
            double surfaceEnergy = energy * surfaceSample * 1.25;
            DepositEnergy(ctx, x, y, surfaceEnergy);

            double reflectivity = Clamp01(hitObj->reflectivity);
            double roughness = Clamp01(hitObj->roughness);
            double specEnergy = surfaceEnergy * reflectivity;
            double diffuseEnergy = surfaceEnergy * (1.0 - reflectivity);

            if (specEnergy > MIN_ENERGY) {
                double rx, ry;
                ReflectDirection(dirX, dirY, nx, ny, &rx, &ry);
                double jitter = Noise2D(x + depth * 13.37, y - depth * 7.31);
                ApplyRoughness(&rx, &ry, roughness, jitter);
                TraceRayRecursive(ctx, x + nx * 0.5, y + ny * 0.5, rx, ry, depth + 1, specEnergy * 0.9);
            }

            if (diffuseEnergy > MIN_ENERGY) {
                double jitter = Noise2D(x * 0.5 + depth * 2.0, y * 0.5 - depth * 3.0);
                double baseAngle = atan2(ny, nx);
                double spread = (0.5 + roughness * 0.5) * M_PI_2;
                double newAngle = baseAngle + (jitter - 0.5) * spread;
                double dx = cos(newAngle);
                double dy = sin(newAngle);
                TraceRayRecursive(ctx, x + nx * 0.2, y + ny * 0.2, dx, dy, depth + 1, diffuseEnergy * 0.7);
            }
            return;
        }

        energy *= decay;
        if (energy < MIN_ENERGY) {
            break;
        }
    }
}

static void EmitRayRange(const TraceContext* ctx,
                         double sourceX,
                         double sourceY,
                         int rayStart,
                         int rayEnd) {
    for (int ray = rayStart; ray < rayEnd; ray++) {
        double angle = (2.0 * M_PI * ray) / sceneSettings.rays;
        double dirX = cos(angle);
        double dirY = sin(angle);
        TraceRayRecursive(ctx, sourceX, sourceY, dirX, dirY, 0, 1.0);
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

    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8)); // Clear buffer
    memset(energyBuffer, 0, pixelCount * sizeof(float));

    // Use the first Bézier path point as the default light position
    if (sceneSettings.bezierPath.numPoints > 0) {
        light.x = sceneSettings.bezierPath.points[0].x;
        light.y = sceneSettings.bezierPath.points[0].y;
        printf("INFO: Light source initialized from Bézier Path at (%.2f, %.2f)\n", light.x, light.y);
    } else {
        // Fallback to hardcoded default if Bézier path is invalid
        light.x = 100;
        light.y = 100;
        printf("WARNING: Bézier Path is uninitialized! Using default light position (100, 100)\n");
    }
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
}

void SetLightPosition(double x, double y) {
    light.x = x;
    light.y = y;
}


void ApplyBlur(Uint8* pixelBuffer, int width, int height) {
    Uint8 tempBuffer[width * height];

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int idx = y * width + x;

            // Simple 3x3 blur kernel (average neighboring pixels)
            int sum = 0;
            sum += pixelBuffer[idx];                      // Center
            sum += pixelBuffer[idx - width];              // Top
            sum += pixelBuffer[idx + width];              // Bottom
            sum += pixelBuffer[idx - 1];                  // Left
            sum += pixelBuffer[idx + 1];                  // Right
            sum += pixelBuffer[idx - width - 1];          // Top-left
            sum += pixelBuffer[idx - width + 1];          // Top-right
            sum += pixelBuffer[idx + width - 1];          // Bottom-left
            sum += pixelBuffer[idx + width + 1];          // Bottom-right

            tempBuffer[idx] = sum / 9; // Average value
        }
    }

    // Copy back to the original buffer
    memcpy(pixelBuffer, tempBuffer, width * height);
}

void ApplyHeavyBlur(Uint8* pixelBuffer, int width, int height) {
    Uint8 tempBuffer[width * height];

    for (int y = 2; y < height - 2; y++) {
        for (int x = 2; x < width - 2; x++) {
            int idx = y * width + x;
            int sum = 0;

            // 5x5 Kernel for stronger blur
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    sum += pixelBuffer[(y + dy) * width + (x + dx)];
                }
            }

            tempBuffer[idx] = sum / 25;  // Average over 25 pixels
        }
    }

    // Copy back to the original buffer
    memcpy(pixelBuffer, tempBuffer, width * height);
}

void ApplyGaussianBlur(Uint8* pixelBuffer, int width, int height) {
    Uint8 tempBuffer[width * height];

    // Gaussian kernel (weights for 5x5 kernel)
    float kernel[5][5] = {
        {1,  4,  7,  4, 1},
        {4, 16, 26, 16, 4},
        {7, 26, 41, 26, 7},
        {4, 16, 26, 16, 4},
        {1,  4,  7,  4, 1}
    };
    float kernelSum = 273.0f;  // Sum of all weights to normalize

    for (int y = 2; y < height - 2; y++) {
        for (int x = 2; x < width - 2; x++) {
            float sum = 0.0f;

            // Apply Gaussian filter
            for (int ky = -2; ky <= 2; ky++) {
                for (int kx = -2; kx <= 2; kx++) {
                    int idx = (y + ky) * width + (x + kx);
                    sum += pixelBuffer[idx] * kernel[ky + 2][kx + 2];
                }
            }

            tempBuffer[y * width + x] = (Uint8)(sum / kernelSum);
        }
    }

    // Copy the blurred data back
    memcpy(pixelBuffer, tempBuffer, width * height);
}

void ApplyGaussianBlurWithEdgePreservation(Uint8* pixelBuffer, int width, int height, SceneObject* objects, int obj_count) {
    Uint8 tempBuffer[width * height];

    float kernel[5][5] = {
        {1,  4,  7,  4, 1},
        {4, 16, 26, 16, 4},
        {7, 26, 41, 26, 7},
        {4, 16, 26, 16, 4},
        {1,  4,  7,  4, 1}
    };   
    float kernelSum = 273.0f;

    for (int y = 2; y < height - 2; y++) {
        for (int x = 2; x < width - 2; x++) {
            int idx = y * width + x;

            // ✅ Check if pixel belongs to any object using `IsInsideObject()`
            bool isObject = false;
            for (int i = 0; i < obj_count; i++) {
                if (IsInsideObject(x, y, &objects[i])) {
                    isObject = true;
                    break;
                }
            }

            // ✅ If the pixel is part of an object, don't blur it
            if (isObject) {
                tempBuffer[idx] = pixelBuffer[idx];
                continue;
            }

            // ✅ Apply Gaussian Blur for non-object pixels
            float sum = 0.0f;
            for (int ky = -2; ky <= 2; ky++) {
                for (int kx = -2; kx <= 2; kx++) {
                    int neighborIdx = (y + ky) * width + (x + kx);
                    sum += pixelBuffer[neighborIdx] * kernel[ky + 2][kx + 2];
                }
            }

            tempBuffer[idx] = (Uint8)(sum / kernelSum);
        }
    }

    memcpy(pixelBuffer, tempBuffer, width * height);
}


void FillRays(const TraceContext* ctx, double x, double y) {
    EmitRayRange(ctx, x, y, 0, sceneSettings.rays);
}

int RayCalculationWorker(void* data) {
    ThreadData* threadData = (ThreadData*)data;
    EmitRayRange(threadData->ctx, threadData->sourceX, threadData->sourceY,
                 threadData->rayStart, threadData->rayEnd);
    return 0;
}

void RenderRayTracingScene(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    size_t pixelCount = (size_t)WIDTH * (size_t)HEIGHT;

    // Ensure pixelBuffer is allocated
    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate pixel buffer during render.\n");
            return;
        }
    }
    if (energyBuffer == NULL) {
        energyBuffer = (float*)malloc(pixelCount * sizeof(float));
        if (!energyBuffer) {
            printf("ERROR: Failed to allocate energy buffer during render.\n");
            return;
        }
    }
    
    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8)); // Clear buffer
    memset(energyBuffer, 0, pixelCount * sizeof(float));

    TraceContext context = {
        .pixelBuffer = pixelBuffer,
        .energyBuffer = energyBuffer,
        .width = WIDTH,
        .height = HEIGHT,
        .objects = sceneSettings.sceneObjects,
        .objectCount = sceneSettings.objectCount
    };

    if (animSettings.lightMode == 0) {
        // Original long-ray mode
        FillRays(&context, light.x, light.y);
    } else if (animSettings.lightMode == 1) {
        const int NUM_THREADS = 4;
        SDL_Thread* threads[NUM_THREADS];
        ThreadData threadData[NUM_THREADS];
        int raysPerThread = sceneSettings.rays / NUM_THREADS;

        for (int i = 0; i < NUM_THREADS; i++) {
            threadData[i] = (ThreadData){
                .sourceX = light.x,
                .sourceY = light.y,
                .rayStart = i * raysPerThread,
                .rayEnd = (i == NUM_THREADS - 1) ? sceneSettings.rays : (i + 1) * raysPerThread,
                .ctx = &context
            };
            threads[i] = SDL_CreateThread(RayCalculationWorker, "RayWorker", &threadData[i]);
        }
        for (int i = 0; i < NUM_THREADS; i++)
            SDL_WaitThread(threads[i], NULL);
    }

    if (animSettings.lightDiffusionEnabled && animSettings.lightDiffusionRadius > 0) {
        ApplyEnergyDiffusion(&context,
                             animSettings.lightDiffusionRadius,
                             animSettings.lightDiffusionStrength);
    }

    ConvertEnergyToPixels(&context);

    if (animSettings.blurMode == 1) {
        ApplyBlur(pixelBuffer, WIDTH, HEIGHT);
    } else if (animSettings.blurMode == 2) {
        ApplyHeavyBlur(pixelBuffer, WIDTH, HEIGHT);
    } else if (animSettings.blurMode == 3) {
        ApplyGaussianBlurWithEdgePreservation(pixelBuffer, WIDTH, HEIGHT,
                                              sceneSettings.sceneObjects, sceneSettings.objectCount);
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

    SDL_RenderPresent(renderer);
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
