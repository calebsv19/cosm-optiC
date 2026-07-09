#include "render/pipeline/ray_tracing2_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "render/irradiance_cache.h"
#include "render/integrators/forward_light_integrator.h"
#include "render/material_bsdf.h"

static void RayTracing2Materials_ApplyOverrides(MaterialBSDF* material) {
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

int RayTracing2Materials_BuildTable(MaterialBSDF** io_material_table, int* io_material_capacity) {
    int count = sceneSettings.objectCount;
    MaterialBSDF* material_table = io_material_table ? *io_material_table : NULL;
    int material_capacity = io_material_capacity ? *io_material_capacity : 0;

    if (count <= 0) {
        if (material_table) {
            free(material_table);
            material_table = NULL;
        }
        if (io_material_table) *io_material_table = NULL;
        if (io_material_capacity) *io_material_capacity = 0;
        return 0;
    }

    if (!material_table || material_capacity < count) {
        MaterialBSDF* new_buffer =
            (MaterialBSDF*)realloc(material_table, (size_t)count * sizeof(MaterialBSDF));
        if (!new_buffer) {
            fprintf(stderr, "ERROR: Failed to allocate material table.\n");
            free(material_table);
            if (io_material_table) *io_material_table = NULL;
            if (io_material_capacity) *io_material_capacity = 0;
            return 0;
        }
        material_table = new_buffer;
        material_capacity = count;
    }

    for (int i = 0; i < count; i++) {
        MaterialBSDFInitFromSceneObject(&sceneSettings.sceneObjects[i], &material_table[i]);
        RayTracing2Materials_ApplyOverrides(&material_table[i]);
    }

    if (io_material_table) *io_material_table = material_table;
    if (io_material_capacity) *io_material_capacity = material_capacity;
    return count;
}

bool RayTracing2Materials_BuildReflectionCache(const IntegratorContext* ctx,
                                               const LightSource* light,
                                               float** io_reflection_forward_buffer) {
    float* reflection_forward_buffer = io_reflection_forward_buffer
                                           ? *io_reflection_forward_buffer
                                           : NULL;
    if (!ctx || !ctx->cache || !io_reflection_forward_buffer) return false;
    int width = ctx->width;
    int height = ctx->height;
    size_t pixelCount = (size_t)width * (size_t)height;
    if (!reflection_forward_buffer) {
        reflection_forward_buffer = (float*)malloc(pixelCount * sizeof(float));
        if (!reflection_forward_buffer) {
            return false;
        }
        *io_reflection_forward_buffer = reflection_forward_buffer;
    }

    int savedRays = sceneSettings.rays;
    int probeRays = savedRays / 6;
    if (probeRays < 128) probeRays = 128;
    sceneSettings.rays = probeRays;

    IntegratorContext probeCtx = *ctx;
    probeCtx.pixelBuffer = NULL;
    probeCtx.energyBuffer = reflection_forward_buffer;
    probeCtx.useTiles = false;
    probeCtx.tileGrid = NULL;
    memset(reflection_forward_buffer, 0, pixelCount * sizeof(float));
    ForwardLightIntegratorRender(&probeCtx, light);
    sceneSettings.rays = savedRays;

    float maxEnergy = 0.0f;
    for (size_t i = 0; i < pixelCount; i++) {
        if (reflection_forward_buffer[i] > maxEnergy) {
            maxEnergy = reflection_forward_buffer[i];
        }
    }
    if (maxEnergy <= 0.0f) {
        maxEnergy = 1.0f;
    }

    bool ok = IrradianceCacheFill(ctx->cache,
                                  ctx->objects,
                                  ctx->objectCount,
                                  light,
                                  ctx->uniformGrid,
                                  reflection_forward_buffer,
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
                               reflection_forward_buffer,
                               width,
                               height,
                               (double)maxEnergy,
                               NULL,
                               0,
                               NULL,
                               true);
}

void RayTracing2Materials_Cleanup(MaterialBSDF** io_material_table,
                                  int* io_material_capacity,
                                  float** io_reflection_forward_buffer) {
    if (io_reflection_forward_buffer && *io_reflection_forward_buffer) {
        free(*io_reflection_forward_buffer);
        *io_reflection_forward_buffer = NULL;
    }
    if (io_material_table && *io_material_table) {
        free(*io_material_table);
        *io_material_table = NULL;
    }
    if (io_material_capacity) {
        *io_material_capacity = 0;
    }
}
