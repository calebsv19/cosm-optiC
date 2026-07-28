#ifndef PROCEDURAL_SOLID_MATERIAL_BINDING_H
#define PROCEDURAL_SOLID_MATERIAL_BINDING_H

#include "procedural/procedural_solid_regions.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SOLID_MATERIAL_BINDING_SCHEMA \
    "ray_tracing.procedural_solid_region_material_binding"
#define PROCEDURAL_SOLID_MATERIAL_BINDING_SCHEMA_VERSION 1u
#define PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY 65u

typedef enum ProceduralSolidMaterialPreset {
    PROCEDURAL_SOLID_MATERIAL_DEFAULT = 0,
    PROCEDURAL_SOLID_MATERIAL_MIRROR = 1,
    PROCEDURAL_SOLID_MATERIAL_ROUGH_METAL = 2,
    PROCEDURAL_SOLID_MATERIAL_GLOSSY = 3,
    PROCEDURAL_SOLID_MATERIAL_EMISSIVE = 4,
    PROCEDURAL_SOLID_MATERIAL_TRANSPARENT = 5,
    PROCEDURAL_SOLID_MATERIAL_INVALID = 6
} ProceduralSolidMaterialPreset;

typedef struct ProceduralSolidRegionMaterialAssignment {
    char region_id[64];
    ProceduralSolidMaterialPreset material;
} ProceduralSolidRegionMaterialAssignment;

typedef struct ProceduralSolidMaterialBindingV1 {
    uint32_t schema_version;
    char binding_id[64];
    char asset_id[64];
    char semantic_source_id[64];
    char mesh_digest_sha256[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
    char region_digest_sha256[PROCEDURAL_SOLID_MESH_DIGEST_CAPACITY];
    ProceduralSolidMaterialPreset fallback_material;
    size_t region_count;
    ProceduralSolidRegionRecord regions[PROCEDURAL_SOLID_REGION_MAX];
    size_t assignment_count;
    ProceduralSolidRegionMaterialAssignment
        assignments[PROCEDURAL_SOLID_REGION_MAX];
} ProceduralSolidMaterialBindingV1;

typedef enum ProceduralSolidMaterialBindingStatus {
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_OK = 0,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_ARGUMENT,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_IO,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_JSON,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_SCHEMA,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_IDENTITY,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_REGION,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_MATERIAL,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_STALE_BASE,
    PROCEDURAL_SOLID_MATERIAL_BINDING_STATUS_CAPACITY
} ProceduralSolidMaterialBindingStatus;

typedef struct ProceduralSolidMaterialBindingReport {
    ProceduralSolidMaterialBindingStatus status;
    char field[96];
    char message[256];
    char binding_digest_sha256[
        PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
} ProceduralSolidMaterialBindingReport;

void ProceduralSolidMaterialBindingV1_Init(
    ProceduralSolidMaterialBindingV1 *binding);

bool ProceduralSolidMaterialBindingV1_FromReceiptFile(
    const char *receipt_path,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *binding_id,
    ProceduralSolidMaterialPreset fallback,
    ProceduralSolidMaterialBindingV1 *out_binding,
    ProceduralSolidMaterialBindingReport *report);

bool ProceduralSolidMaterialBindingV1_LoadJsonFile(
    const char *path,
    ProceduralSolidMaterialBindingV1 *out_binding,
    ProceduralSolidMaterialBindingReport *report);

bool ProceduralSolidMaterialBindingV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralSolidMaterialBindingV1 *binding,
    ProceduralSolidMaterialBindingReport *report);

bool ProceduralSolidMaterialBindingV1_Validate(
    const ProceduralSolidMaterialBindingV1 *binding,
    const CoreMeshAssetRuntimeDocument *mesh,
    ProceduralSolidMaterialBindingReport *report);

bool ProceduralSolidMaterialBindingV1_Digest(
    const ProceduralSolidMaterialBindingV1 *binding,
    char out_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY],
    ProceduralSolidMaterialBindingReport *report);

bool ProceduralSolidMaterialBindingV1_AssignRegion(
    const ProceduralSolidMaterialBindingV1 *base,
    const char *expected_base_digest,
    const char *region_id,
    ProceduralSolidMaterialPreset material,
    ProceduralSolidMaterialBindingV1 *out_binding,
    ProceduralSolidMaterialBindingReport *report);

bool ProceduralSolidMaterialBindingV1_AssignKind(
    const ProceduralSolidMaterialBindingV1 *base,
    const char *expected_base_digest,
    ProceduralSolidRegionKind kind,
    ProceduralSolidMaterialPreset material,
    ProceduralSolidMaterialBindingV1 *out_binding,
    ProceduralSolidMaterialBindingReport *report);

bool ProceduralSolidMaterialBindingV1_Resolve(
    const ProceduralSolidMaterialBindingV1 *binding,
    const char *region_id,
    ProceduralSolidMaterialPreset *out_material,
    bool *out_used_fallback);

const char *ProceduralSolidMaterialPreset_Name(
    ProceduralSolidMaterialPreset material);
ProceduralSolidMaterialPreset ProceduralSolidMaterialPreset_Parse(
    const char *name);
const char *ProceduralSolidMaterialBindingStatus_Name(
    ProceduralSolidMaterialBindingStatus status);

#endif
