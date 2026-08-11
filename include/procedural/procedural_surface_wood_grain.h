#ifndef PROCEDURAL_SURFACE_WOOD_GRAIN_H
#define PROCEDURAL_SURFACE_WOOD_GRAIN_H

#include <stdbool.h>
#include <stddef.h>

#define PROCEDURAL_SURFACE_WOOD_GRAIN_DIGEST_CAPACITY 65

typedef struct ProceduralSurfaceWoodGrainFieldV1 {
    char preset_digest_sha256[PROCEDURAL_SURFACE_WOOD_GRAIN_DIGEST_CAPACITY];
    char source_mesh_digest_sha256[PROCEDURAL_SURFACE_WOOD_GRAIN_DIGEST_CAPACITY];
    double orientation_radians, frequency_per_unit, width_variation, turbulence;
    int flow_kind; /* 0 straight, 1 curved, 2 turbulent */
    double base_color[3], latewood_color[3], contrast;
    double normal_strength, knot_normal_strength;
} ProceduralSurfaceWoodGrainFieldV1;

typedef struct ProceduralSurfaceWoodGrainSampleV1 {
    double color[3], roughness_delta, slope_x, slope_z;
    double height;
} ProceduralSurfaceWoodGrainSampleV1;

bool ProceduralSurfaceWoodGrainFieldV1_LoadJsonFile(const char *path,
    ProceduralSurfaceWoodGrainFieldV1 *out_field);
bool ProceduralSurfaceWoodGrainFieldV1_Validate(
    const ProceduralSurfaceWoodGrainFieldV1 *field);
bool ProceduralSurfaceWoodGrainFieldV1_Sample(
    const ProceduralSurfaceWoodGrainFieldV1 *field, double x, double z,
    ProceduralSurfaceWoodGrainSampleV1 *out_sample);

#endif
