#ifndef RAYTRACING_H
#define RAYTRACING_H

#include <SDL2/SDL.h>
#include "render/integrator_common.h"

// Define a simple circle type used in the scene.
typedef struct {
    double x;
    double y;
    double r;
} Circle;

// Initializes the raytracing scene.
void InitRayTracingScene(void);

// Renders the raytracing scene using the given light source position.
void RenderRayTracingScene(SDL_Renderer* renderer);

// Processes interactive events (e.g., mouse drag) to update the light source position.
void ProcessRayTracingEvent(SDL_Event* event);

// Returns the current light source position (for interactive mode).
void GetCurrentLightPosition(double* x, double* y);

// clears buffer memory
void CleanupRayTracing(void);

// ray_tracing2.h
void SetLightPosition(double x, double y);

typedef struct {
    IrradianceCache cache;
    Uint8* forwardBuffer;
} ReflectionData;

#endif // RAYTRACING_H
