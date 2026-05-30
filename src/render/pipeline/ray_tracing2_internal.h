#ifndef RAY_TRACING2_INTERNAL_H
#define RAY_TRACING2_INTERNAL_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "core_space.h"
#include "render/integrator_common.h"
#include "render/material_bsdf.h"
#include "render/ray_tracing2.h"

bool RayTracing2Fluid_InitializeScene(void);
void RayTracing2Fluid_CleanupScene(void);
void RayTracing2Fluid_ClampCameraToGrid(void);
void RayTracing2Fluid_RenderOverlay(SDL_Renderer* renderer, int width, int height);

int RayTracing2Materials_BuildTable(MaterialBSDF** io_material_table, int* io_material_capacity);
bool RayTracing2Materials_BuildReflectionCache(const IntegratorContext* ctx,
                                               const LightSource* light,
                                               float** io_reflection_forward_buffer);
void RayTracing2Materials_Cleanup(MaterialBSDF** io_material_table,
                                  int* io_material_capacity,
                                  float** io_reflection_forward_buffer);

#endif
