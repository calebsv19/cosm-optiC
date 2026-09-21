#ifndef CORE_AUTHORED_SURFACE_MAPPING_H
#define CORE_AUTHORED_SURFACE_MAPPING_H
#include <stdbool.h>
#include <stdint.h>

/* Additive, JSON-free meaning. Hosts own transforms, IO and texture sampling.
 * v1 supports planar coordinates only. No implicit UV/axial fallback. */
#define CORE_AUTHORED_SURFACE_MAPPING_VERSION 1u
typedef enum CoreAuthoredSurfaceSpace {
    CORE_AUTHORED_SURFACE_OBJECT_REST = 1,
    CORE_AUTHORED_SURFACE_WORLD = 2
} CoreAuthoredSurfaceSpace;
typedef struct CoreAuthoredSurfaceMapping {
    uint32_t version;
    CoreAuthoredSurfaceSpace space;
    double origin_m[3], axis_u[3], axis_v[3];
    double tile_m[2], offset_m[2], pivot_m[2], rotation_rad;
    uint32_t seed;
} CoreAuthoredSurfaceMapping;
typedef struct CoreAuthoredSurfaceCoordinates {
    double uv_tiles[2]; /* unwrapped; addressing belongs to the source */
    bool valid;
    bool singular;
    /* Reserved data are absent, never implicitly zero-valued attributes. */
    bool has_tangent, has_footprint, has_authored_uv;
} CoreAuthoredSurfaceCoordinates;

bool core_authored_surface_mapping_validate(const CoreAuthoredSurfaceMapping* mapping);
#endif
