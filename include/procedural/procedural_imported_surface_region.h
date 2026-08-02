#ifndef PROCEDURAL_IMPORTED_SURFACE_REGION_H
#define PROCEDURAL_IMPORTED_SURFACE_REGION_H

#include "core_mesh_asset.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_IMPORTED_SURFACE_REGION_SCHEMA \
    "ray_tracing.procedural_imported_surface_region"
#define PROCEDURAL_IMPORTED_SURFACE_REGION_SCHEMA_VERSION 1u
#define PROCEDURAL_IMPORTED_SURFACE_REGION_RECIPE_SCHEMA \
    "ray_tracing.procedural_imported_surface_region_recipe"
#define PROCEDURAL_IMPORTED_SURFACE_REGION_RECIPE_SCHEMA_VERSION 1u
#define PROCEDURAL_IMPORTED_SURFACE_REGION_ID_CAPACITY 64u
#define PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY 65u
#define PROCEDURAL_IMPORTED_SURFACE_REGION_MAX_PATCHES 8u

typedef struct ProceduralImportedSurfaceRegionPatchV1 {
    double center[3];
    double radius[3];
    double feather;
    double noise_scale;
    double noise_strength;
    double strength;
    int seed;
} ProceduralImportedSurfaceRegionPatchV1;

typedef struct ProceduralImportedSurfaceRegionRecipeV1 {
    uint32_t schema_version;
    char region_id[PROCEDURAL_IMPORTED_SURFACE_REGION_ID_CAPACITY];
    char source_asset_id[PROCEDURAL_IMPORTED_SURFACE_REGION_ID_CAPACITY];
    size_t patch_count;
    ProceduralImportedSurfaceRegionPatchV1
        patches[PROCEDURAL_IMPORTED_SURFACE_REGION_MAX_PATCHES];
} ProceduralImportedSurfaceRegionRecipeV1;

typedef struct ProceduralImportedSurfaceRegionV1 {
    uint32_t schema_version;
    char region_id[PROCEDURAL_IMPORTED_SURFACE_REGION_ID_CAPACITY];
    char source_asset_id[PROCEDURAL_IMPORTED_SURFACE_REGION_ID_CAPACITY];
    char source_mesh_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY];
    char source_file_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY];
    char recipe_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY];
    char value_digest_sha256[
        PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY];
    size_t vertex_count;
    size_t triangle_count;
    double minimum;
    double maximum;
    double mean;
    size_t transition_vertex_count;
    bool topology_unchanged;
    bool source_triangle_provenance_retained;
    double *vertex_weights;
} ProceduralImportedSurfaceRegionV1;

typedef struct ProceduralImportedSurfaceRegionReport {
    bool ok;
    char field[96];
    char message[256];
} ProceduralImportedSurfaceRegionReport;

void ProceduralImportedSurfaceRegionV1_Init(
    ProceduralImportedSurfaceRegionV1 *region);
void ProceduralImportedSurfaceRegionV1_Free(
    ProceduralImportedSurfaceRegionV1 *region);
bool ProceduralImportedSurfaceRegionV1_RefreshValues(
    ProceduralImportedSurfaceRegionV1 *region);
bool ProceduralImportedSurfaceRegionRecipeV1_LoadJsonFile(
    const char *path,
    ProceduralImportedSurfaceRegionRecipeV1 *out_recipe,
    ProceduralImportedSurfaceRegionReport *report);
bool ProceduralImportedSurfaceRegionV1_Compile(
    const ProceduralImportedSurfaceRegionRecipeV1 *recipe,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *source_runtime_path,
    ProceduralImportedSurfaceRegionV1 *out_region,
    ProceduralImportedSurfaceRegionReport *report);
bool ProceduralImportedSurfaceRegionV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralImportedSurfaceRegionV1 *region,
    ProceduralImportedSurfaceRegionReport *report);
bool ProceduralImportedSurfaceRegionV1_LoadJsonFile(
    const char *path,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *source_runtime_path,
    ProceduralImportedSurfaceRegionV1 *out_region,
    ProceduralImportedSurfaceRegionReport *report);
bool ProceduralImportedSurfaceRegionV1_ValidateForMesh(
    const ProceduralImportedSurfaceRegionV1 *region,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *source_runtime_path,
    ProceduralImportedSurfaceRegionReport *report);

#endif
