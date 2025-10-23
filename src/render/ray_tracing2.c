#include "render/ray_tracing2.h"
#include "config/config_manager.h"  // Include animation.h to access `sceneObjects[]` and `objectCount`
#include "render/render_helper.h"
#include "editor/scene_editor.h"
#include "app/animation.h"
#include <stdio.h>   
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <math.h>

typedef struct {
    double sourceX, sourceY;
    int rayStart, rayEnd;
    SceneObject* objects;
    int objectCount;
    Uint8* pixelBuffer; // brightness values
    int width, height;
} ThreadData;


// Global light source.
static Circle light = {100, 100, 10};  // Default position (updated dynamically)
const int TOTAL_RAYS = 1800;
static double INTENSITY = 6000000.0;
Uint8* pixelBuffer = NULL;  // Global but uninitialized


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

    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(WIDTH * HEIGHT * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate memory for pixel buffer!\n");
            exit(1);
        }
    }

    memset(pixelBuffer, 0, WIDTH * HEIGHT * sizeof(Uint8)); // Clear buffer

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


void FillRays(double x, double y, SceneObject* objects, int obj_count,
              Uint8* pixelBuffer, int width, int height) {

    double angle_increment = (2.0 * M_PI) / sceneSettings.rays;

    for (int i = 0; i < sceneSettings.rays; i++) {
        double angle = i * angle_increment;
        double x_dir = cos(angle);
        double y_dir = sin(angle);

        double step = 1.0;
        double x_draw = x;
        double y_draw = y;
        double weight = 1.0;
        double decay = 0.9965;  // Light fades over distance
        int end = 0;

        while (!end) {
            x_draw += step * x_dir;
            y_draw += step * y_dir;

            // Stop if out of screen bounds
            if (x_draw < 0 || x_draw >= width || y_draw < 0 || y_draw >= height) {
                end = 1;
                break;
            }

            // Stop if colliding with any scene object
            for (int j = 0; j < obj_count; j++) {
		if (IsInsideObject((int)x_draw, (int)y_draw, &objects[j])) {
                    end = 1;
                    break;
                }
            }

            // Light decay logic
            int brightness = (int)(215 * weight);
            if (brightness < 0) brightness = 0;
            if (brightness > 215) brightness = 215;
		
            int idx = (int)y_draw * width + (int)x_draw;
            if (idx >= 0 && idx < width * height) {
                if (pixelBuffer[idx] < brightness) {
                    pixelBuffer[idx] = brightness;
                }
            }
	
            weight *= decay;  // Gradual light fade
        }
    }
}

int RayCalculationWorker(void* data) {
    ThreadData* threadData = (ThreadData*)data;

    for (int ray = threadData->rayStart; ray < threadData->rayEnd; ray++) {
        double angle = (2.0 * M_PI * ray) / sceneSettings.rays;
        double x_dir = cos(angle), y_dir = sin(angle);

        double x_draw = threadData->sourceX;
        double y_draw = threadData->sourceY;
        double weight = 1.0;

        while (weight > 0.01) {
            x_draw += x_dir;
            y_draw += y_dir;

	
            if (x_draw < 0 || x_draw >= threadData->width || y_draw < 0 || y_draw >= threadData->height)
                break;


            // Check collision once per step
            bool collided = false;
            for (int j = 0; j < threadData->objectCount; j++) {
		if (IsInsideObject((int)x_draw, (int)y_draw, &threadData->objects[j])) {
                    collided = true;
                    break;
                }
            }
            if (collided) break;

	    // Compute squared distance (avoiding sqrt for performance)
	    double dx = x_draw - threadData->sourceX;
	    double dy = y_draw - threadData->sourceY;
	    double distance_sq = dx * dx + dy * dy;
	
	    // Define safe minimum distance to prevent extreme brightness
	    const double min_distance_sq = 100.0;  // Prevents harsh brightness near source
	    if (distance_sq < min_distance_sq) distance_sq = min_distance_sq;

	    // Adjust falloff to be more gradual across the screen
	    const double falloff_factor = INTENSITY;  // Tweak this for global brightness control
            double intensity = falloff_factor / (distance_sq + falloff_factor / 255.0); 

	    // Normalize brightness to prevent extreme hotspots
	    if (intensity > 255.0) intensity = 255.0;

	    // Apply smooth brightness accumulation over multiple rays
            int idx = (int)y_draw * threadData->width + (int)x_draw;
	    if (idx < 0 || idx >= threadData->width * threadData->height)
		    continue;

	    Uint8 new_brightness = (Uint8)intensity;

	    // Accumulate brightness without overpowering (soft blending)
	    if (threadData->pixelBuffer[idx] < new_brightness) {
		    threadData->pixelBuffer[idx] = (threadData->pixelBuffer[idx] + new_brightness) / 2;
	    }
	}
    }
    return 0;
}

void RenderRayTracingScene(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    // Ensure pixelBuffer is allocated
    if (pixelBuffer == NULL) {
        printf("ERROR: pixelBuffer is NULL! Ensure InitRayTracingScene() was called.\n");
        return;
    }
    
    memset(pixelBuffer, 0, WIDTH * HEIGHT * sizeof(Uint8)); // Clear buffer

    if (animSettings.lightMode == 0) {
        // Original long-ray mode
        FillRays(light.x, light.y, sceneSettings.sceneObjects, sceneSettings.objectCount,
                 pixelBuffer, WIDTH, HEIGHT);
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
                .objects = sceneSettings.sceneObjects,
                .objectCount = sceneSettings.objectCount,
                .pixelBuffer = pixelBuffer,
                .width = WIDTH,
                .height = HEIGHT
            };
            threads[i] = SDL_CreateThread(RayCalculationWorker, "RayWorker", &threadData[i]);
        }
        for (int i = 0; i < NUM_THREADS; i++)
            SDL_WaitThread(threads[i], NULL);
        
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                Uint8 brightness = pixelBuffer[y * WIDTH + x];
                if (brightness > 0) {
                    SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
                    SDL_RenderDrawPoint(renderer, x, y);
                }
            }
        }
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    // ✅ Draw the light source
    RenderCircle(renderer, light.x, light.y, light.r, true);


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
        light.x = event->motion.x;
        light.y = event->motion.y;
    }
    if (event->key.keysym.sym == SDLK_b) {  // Press "B" to switch blur mode
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
