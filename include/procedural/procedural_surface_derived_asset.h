#ifndef PROCEDURAL_SURFACE_DERIVED_ASSET_H
#define PROCEDURAL_SURFACE_DERIVED_ASSET_H

#include "procedural/procedural_surface_material.h"
#include "procedural/procedural_surface_plane_mesh.h"

#include <stdbool.h>
#include <stddef.h>

#define PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA \
    "ray_tracing.procedural_surface_derived_asset"
#define PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA_VERSION_V1 1u
#define PROCEDURAL_SURFACE_DERIVED_ASSET_SCHEMA_VERSION 2u
#define PROCEDURAL_SURFACE_DERIVED_ASSET_PATH_CAPACITY 4096u
#define PROCEDURAL_SURFACE_DERIVED_ASSET_ID_CAPACITY 64u
#define PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY 65u

typedef struct ProceduralSurfaceDerivedAssetManifest {
    unsigned int schema_version;
    char asset_id[PROCEDURAL_SURFACE_DERIVED_ASSET_ID_CAPACITY];
    char source_asset_id[PROCEDURAL_SURFACE_DERIVED_ASSET_ID_CAPACITY];
    char recipe_path[PROCEDURAL_SURFACE_DERIVED_ASSET_PATH_CAPACITY];
    char field_graph_path[PROCEDURAL_SURFACE_DERIVED_ASSET_PATH_CAPACITY];
    char binding_path[PROCEDURAL_SURFACE_DERIVED_ASSET_PATH_CAPACITY];
    char mesh_path[PROCEDURAL_SURFACE_DERIVED_ASSET_PATH_CAPACITY];
    char material_path[PROCEDURAL_SURFACE_DERIVED_ASSET_PATH_CAPACITY];
    char recipe_digest_sha256[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char field_graph_digest_sha256[
        PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char binding_digest_sha256[
        PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char cage_digest_sha256[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char shell_digest_sha256[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char material_digest_sha256[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    char cache_identity_sha256[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY];
    ProceduralSurfacePlaneQuality quality;
    ProceduralSurfaceCageKind cage_kind;
    double cage_width_units;
    double cage_height_units;
    double cage_depth_units;
    char collision_owner[32];
} ProceduralSurfaceDerivedAssetManifest;

typedef struct ProceduralSurfaceDerivedAssetMaterial {
    bool valid;
    size_t vertex_count;
    size_t triangle_count;
    ProceduralSurfaceMaterialSample *vertex_samples;
    unsigned int *triangle_indices;
} ProceduralSurfaceDerivedAssetMaterial;

typedef enum ProceduralSurfaceDerivedAssetStatus {
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_OK = 0,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_SCHEMA,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IDENTITY,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_PATH,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_IO,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_JSON,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_STALE,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_ALLOCATION,
    PROCEDURAL_SURFACE_DERIVED_ASSET_STATUS_MATERIAL
} ProceduralSurfaceDerivedAssetStatus;

typedef struct ProceduralSurfaceDerivedAssetReport {
    ProceduralSurfaceDerivedAssetStatus status;
    char field[64];
    char message[192];
} ProceduralSurfaceDerivedAssetReport;

void ProceduralSurfaceDerivedAssetMaterial_Init(
    ProceduralSurfaceDerivedAssetMaterial *material);
void ProceduralSurfaceDerivedAssetMaterial_Free(
    ProceduralSurfaceDerivedAssetMaterial *material);

bool ProceduralSurfaceDerivedAsset_CageDigest(
    ProceduralSurfaceCageKind kind,
    double width_units,
    double height_units,
    double depth_units,
    char out_digest[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY],
    ProceduralSurfaceDerivedAssetReport *report);

bool ProceduralSurfaceDerivedAsset_CacheIdentity(
    const char *recipe_digest,
    const char *cage_digest,
    ProceduralSurfacePlaneQuality quality,
    const char *shell_digest,
    const char *material_digest,
    char out_digest[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY],
    ProceduralSurfaceDerivedAssetReport *report);

bool ProceduralSurfaceDerivedAsset_CacheIdentityV2(
    const char *recipe_digest,
    const char *field_graph_digest,
    const char *binding_digest,
    const char *cage_digest,
    ProceduralSurfacePlaneQuality quality,
    const char *shell_digest,
    const char *material_digest,
    char out_digest[PROCEDURAL_SURFACE_DERIVED_ASSET_DIGEST_CAPACITY],
    ProceduralSurfaceDerivedAssetReport *report);

bool ProceduralSurfaceDerivedAssetManifest_Validate(
    const ProceduralSurfaceDerivedAssetManifest *manifest,
    ProceduralSurfaceDerivedAssetReport *report);
bool ProceduralSurfaceDerivedAssetManifest_SaveJsonFile(
    const char *path,
    const ProceduralSurfaceDerivedAssetManifest *manifest,
    ProceduralSurfaceDerivedAssetReport *report);
bool ProceduralSurfaceDerivedAssetManifest_LoadJsonFile(
    const char *path,
    ProceduralSurfaceDerivedAssetManifest *out_manifest,
    ProceduralSurfaceDerivedAssetReport *report);

bool ProceduralSurfaceDerivedAssetMaterial_LoadJsonFile(
    const char *path,
    const ProceduralSurfaceDerivedAssetManifest *manifest,
    size_t expected_vertex_count,
    size_t expected_triangle_count,
    ProceduralSurfaceDerivedAssetMaterial *out_material,
    ProceduralSurfaceDerivedAssetReport *report);

const char *ProceduralSurfaceDerivedAssetQuality_Name(
    ProceduralSurfacePlaneQuality quality);
const char *ProceduralSurfaceDerivedAssetStatus_Name(
    ProceduralSurfaceDerivedAssetStatus status);

#endif
