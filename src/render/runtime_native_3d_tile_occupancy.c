#include "render/runtime_native_3d_tile_occupancy.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int runtime_native_3d_tile_occupancy_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void RuntimeNative3DTileOccupancy_Init(RuntimeNative3DTileOccupancy* occupancy) {
    if (!occupancy) return;
    memset(occupancy, 0, sizeof(*occupancy));
}

void RuntimeNative3DTileOccupancy_Free(RuntimeNative3DTileOccupancy* occupancy) {
    if (!occupancy) return;
    free(occupancy->tiles);
    memset(occupancy, 0, sizeof(*occupancy));
}

static void runtime_native_3d_tile_occupancy_disable(RuntimeNative3DTileOccupancy* occupancy) {
    RuntimeNative3DTileOccupancy_Free(occupancy);
}

static bool runtime_native_3d_tile_occupancy_mark_triangle(RuntimeNative3DTileOccupancy* occupancy,
                                                           const RuntimeCameraProjector3D* projector,
                                                           const RuntimeTriangle3D* triangle) {
    double sx0 = 0.0;
    double sy0 = 0.0;
    double sx1 = 0.0;
    double sy1 = 0.0;
    double sx2 = 0.0;
    double sy2 = 0.0;
    double depth = 0.0;
    bool inside = false;
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    int pixel_min_x = 0;
    int pixel_max_x = 0;
    int pixel_min_y = 0;
    int pixel_max_y = 0;
    int tile_min_x = 0;
    int tile_max_x = 0;
    int tile_min_y = 0;
    int tile_max_y = 0;

    if (!occupancy || !projector || !triangle) return false;
    if (!RuntimeCameraProjector3D_ProjectPoint(projector, triangle->p0, &sx0, &sy0, &depth, &inside) ||
        !RuntimeCameraProjector3D_ProjectPoint(projector, triangle->p1, &sx1, &sy1, &depth, &inside) ||
        !RuntimeCameraProjector3D_ProjectPoint(projector, triangle->p2, &sx2, &sy2, &depth, &inside)) {
        return false;
    }

    min_x = fmin(sx0, fmin(sx1, sx2));
    max_x = fmax(sx0, fmax(sx1, sx2));
    min_y = fmin(sy0, fmin(sy1, sy2));
    max_y = fmax(sy0, fmax(sy1, sy2));

    pixel_min_x = (int)floor(min_x);
    pixel_max_x = (int)ceil(max_x);
    pixel_min_y = (int)floor(min_y);
    pixel_max_y = (int)ceil(max_y);

    if (pixel_max_x <= 0 || pixel_max_y <= 0 ||
        pixel_min_x >= occupancy->viewportWidth ||
        pixel_min_y >= occupancy->viewportHeight) {
        return true;
    }

    pixel_min_x = runtime_native_3d_tile_occupancy_clamp_int(pixel_min_x,
                                                             0,
                                                             occupancy->viewportWidth - 1);
    pixel_max_x = runtime_native_3d_tile_occupancy_clamp_int(pixel_max_x,
                                                             pixel_min_x + 1,
                                                             occupancy->viewportWidth);
    pixel_min_y = runtime_native_3d_tile_occupancy_clamp_int(pixel_min_y,
                                                             0,
                                                             occupancy->viewportHeight - 1);
    pixel_max_y = runtime_native_3d_tile_occupancy_clamp_int(pixel_max_y,
                                                             pixel_min_y + 1,
                                                             occupancy->viewportHeight);

    tile_min_x = pixel_min_x / occupancy->tileSize;
    tile_max_x = (pixel_max_x - 1) / occupancy->tileSize;
    tile_min_y = pixel_min_y / occupancy->tileSize;
    tile_max_y = (pixel_max_y - 1) / occupancy->tileSize;

    tile_min_x = runtime_native_3d_tile_occupancy_clamp_int(tile_min_x, 0, occupancy->tilesX - 1);
    tile_max_x = runtime_native_3d_tile_occupancy_clamp_int(tile_max_x, tile_min_x, occupancy->tilesX - 1);
    tile_min_y = runtime_native_3d_tile_occupancy_clamp_int(tile_min_y, 0, occupancy->tilesY - 1);
    tile_max_y = runtime_native_3d_tile_occupancy_clamp_int(tile_max_y, tile_min_y, occupancy->tilesY - 1);

    for (int ty = tile_min_y; ty <= tile_max_y; ++ty) {
        for (int tx = tile_min_x; tx <= tile_max_x; ++tx) {
            occupancy->tiles[(size_t)ty * (size_t)occupancy->tilesX + (size_t)tx] = 1u;
        }
    }
    return true;
}

static bool runtime_native_3d_tile_occupancy_mark_emitter(RuntimeNative3DTileOccupancy* occupancy,
                                                          const RuntimeCameraProjector3D* projector,
                                                          const RuntimeScene3D* scene) {
    double screen_x = 0.0;
    double screen_y = 0.0;
    double camera_depth = 0.0;
    bool inside = false;
    double radius_pixels = 0.0;
    int pixel_min_x = 0;
    int pixel_max_x = 0;
    int pixel_min_y = 0;
    int pixel_max_y = 0;
    int tile_min_x = 0;
    int tile_max_x = 0;
    int tile_min_y = 0;
    int tile_max_y = 0;

    if (!occupancy || !projector || !scene || !scene->hasLight) return true;
    if (!(scene->light.radius > 1e-9)) return true;
    if (!RuntimeCameraProjector3D_ProjectPoint(projector,
                                               scene->light.position,
                                               &screen_x,
                                               &screen_y,
                                               &camera_depth,
                                               &inside) ||
        camera_depth <= projector->nearPlane) {
        return true;
    }

    radius_pixels = scene->light.radius *
                    ((double)projector->viewportWidth /
                     (2.0 * projector->tanHalfFovX * camera_depth));
    if (!(radius_pixels > 0.0) || !isfinite(radius_pixels)) {
        return true;
    }

    pixel_min_x = (int)floor(screen_x - radius_pixels);
    pixel_max_x = (int)ceil(screen_x + radius_pixels);
    pixel_min_y = (int)floor(screen_y - radius_pixels);
    pixel_max_y = (int)ceil(screen_y + radius_pixels);

    if (pixel_max_x <= 0 || pixel_max_y <= 0 ||
        pixel_min_x >= occupancy->viewportWidth ||
        pixel_min_y >= occupancy->viewportHeight) {
        return true;
    }

    pixel_min_x = runtime_native_3d_tile_occupancy_clamp_int(pixel_min_x,
                                                             0,
                                                             occupancy->viewportWidth - 1);
    pixel_max_x = runtime_native_3d_tile_occupancy_clamp_int(pixel_max_x,
                                                             pixel_min_x + 1,
                                                             occupancy->viewportWidth);
    pixel_min_y = runtime_native_3d_tile_occupancy_clamp_int(pixel_min_y,
                                                             0,
                                                             occupancy->viewportHeight - 1);
    pixel_max_y = runtime_native_3d_tile_occupancy_clamp_int(pixel_max_y,
                                                             pixel_min_y + 1,
                                                             occupancy->viewportHeight);

    tile_min_x = pixel_min_x / occupancy->tileSize;
    tile_max_x = (pixel_max_x - 1) / occupancy->tileSize;
    tile_min_y = pixel_min_y / occupancy->tileSize;
    tile_max_y = (pixel_max_y - 1) / occupancy->tileSize;

    tile_min_x = runtime_native_3d_tile_occupancy_clamp_int(tile_min_x, 0, occupancy->tilesX - 1);
    tile_max_x = runtime_native_3d_tile_occupancy_clamp_int(tile_max_x, tile_min_x, occupancy->tilesX - 1);
    tile_min_y = runtime_native_3d_tile_occupancy_clamp_int(tile_min_y, 0, occupancy->tilesY - 1);
    tile_max_y = runtime_native_3d_tile_occupancy_clamp_int(tile_max_y, tile_min_y, occupancy->tilesY - 1);

    for (int ty = tile_min_y; ty <= tile_max_y; ++ty) {
        for (int tx = tile_min_x; tx <= tile_max_x; ++tx) {
            occupancy->tiles[(size_t)ty * (size_t)occupancy->tilesX + (size_t)tx] = 1u;
        }
    }
    return true;
}

bool RuntimeNative3DTileOccupancy_Build(RuntimeNative3DTileOccupancy* occupancy,
                                        const RuntimeScene3D* scene,
                                        const RuntimeCameraProjector3D* projector,
                                        int tile_size) {
    int tiles_x = 0;
    int tiles_y = 0;
    size_t tile_count = 0;

    if (!occupancy || !scene || !projector || tile_size <= 0 ||
        projector->viewportWidth <= 0 || projector->viewportHeight <= 0) {
        runtime_native_3d_tile_occupancy_disable(occupancy);
        return false;
    }

    runtime_native_3d_tile_occupancy_disable(occupancy);

    tiles_x = (projector->viewportWidth + tile_size - 1) / tile_size;
    tiles_y = (projector->viewportHeight + tile_size - 1) / tile_size;
    tile_count = (size_t)tiles_x * (size_t)tiles_y;

    occupancy->tiles = (uint8_t*)calloc(tile_count, sizeof(uint8_t));
    if (!occupancy->tiles) {
        runtime_native_3d_tile_occupancy_disable(occupancy);
        return false;
    }

    occupancy->tileSize = tile_size;
    occupancy->tilesX = tiles_x;
    occupancy->tilesY = tiles_y;
    occupancy->viewportWidth = projector->viewportWidth;
    occupancy->viewportHeight = projector->viewportHeight;
    occupancy->tileCount = tile_count;
    occupancy->valid = true;

    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        if (!runtime_native_3d_tile_occupancy_mark_triangle(occupancy,
                                                            projector,
                                                            &scene->triangleMesh.triangles[i])) {
            runtime_native_3d_tile_occupancy_disable(occupancy);
            return false;
        }
    }
    if (!runtime_native_3d_tile_occupancy_mark_emitter(occupancy, projector, scene)) {
        runtime_native_3d_tile_occupancy_disable(occupancy);
        return false;
    }
    return true;
}

bool RuntimeNative3DTileOccupancy_RegionMayContainGeometry(
    const RuntimeNative3DTileOccupancy* occupancy,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    int tile_min_x = 0;
    int tile_max_x = 0;
    int tile_min_y = 0;
    int tile_max_y = 0;

    if (!occupancy || !occupancy->valid || !occupancy->tiles || occupancy->tileSize <= 0) {
        return true;
    }
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;
    if (end_x > occupancy->viewportWidth) end_x = occupancy->viewportWidth;
    if (end_y > occupancy->viewportHeight) end_y = occupancy->viewportHeight;
    if (start_x >= end_x || start_y >= end_y) {
        return false;
    }

    tile_min_x = runtime_native_3d_tile_occupancy_clamp_int(start_x / occupancy->tileSize,
                                                             0,
                                                             occupancy->tilesX - 1);
    tile_max_x = runtime_native_3d_tile_occupancy_clamp_int((end_x - 1) / occupancy->tileSize,
                                                             tile_min_x,
                                                             occupancy->tilesX - 1);
    tile_min_y = runtime_native_3d_tile_occupancy_clamp_int(start_y / occupancy->tileSize,
                                                             0,
                                                             occupancy->tilesY - 1);
    tile_max_y = runtime_native_3d_tile_occupancy_clamp_int((end_y - 1) / occupancy->tileSize,
                                                             tile_min_y,
                                                             occupancy->tilesY - 1);

    for (int ty = tile_min_y; ty <= tile_max_y; ++ty) {
        for (int tx = tile_min_x; tx <= tile_max_x; ++tx) {
            if (occupancy->tiles[(size_t)ty * (size_t)occupancy->tilesX + (size_t)tx] != 0u) {
                return true;
            }
        }
    }
    return false;
}
