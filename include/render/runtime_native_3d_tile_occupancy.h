#ifndef RENDER_RUNTIME_NATIVE_3D_TILE_OCCUPANCY_H
#define RENDER_RUNTIME_NATIVE_3D_TILE_OCCUPANCY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_scene_3d.h"

typedef struct {
    uint8_t* tiles;
    int tileSize;
    int tilesX;
    int tilesY;
    int viewportWidth;
    int viewportHeight;
    size_t tileCount;
    bool valid;
} RuntimeNative3DTileOccupancy;

void RuntimeNative3DTileOccupancy_Init(RuntimeNative3DTileOccupancy* occupancy);
void RuntimeNative3DTileOccupancy_Free(RuntimeNative3DTileOccupancy* occupancy);
bool RuntimeNative3DTileOccupancy_Build(RuntimeNative3DTileOccupancy* occupancy,
                                        const RuntimeScene3D* scene,
                                        const RuntimeCameraProjector3D* projector,
                                        int tile_size);
bool RuntimeNative3DTileOccupancy_RegionMayContainGeometry(
    const RuntimeNative3DTileOccupancy* occupancy,
    int start_x,
    int start_y,
    int end_x,
    int end_y);

#endif
