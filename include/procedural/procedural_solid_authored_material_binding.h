#ifndef PROCEDURAL_SOLID_AUTHORED_MATERIAL_BINDING_H
#define PROCEDURAL_SOLID_AUTHORED_MATERIAL_BINDING_H

#include "procedural/procedural_solid_authored_material.h"
#include "procedural/procedural_solid_material_binding.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SOLID_AUTHORED_BINDING_SCHEMA \
    "ray_tracing.procedural_solid_authored_material_binding"
#define PROCEDURAL_SOLID_AUTHORED_BINDING_SCHEMA_VERSION 1u
#define PROCEDURAL_SOLID_AUTHORED_BINDING_PATH_CAPACITY 512u

typedef struct ProceduralSolidAuthoredMaterialReferenceV1 {
    char region_id[64];
    char material_id[PROCEDURAL_SOLID_AUTHORED_MATERIAL_ID_CAPACITY];
    char material_path[PROCEDURAL_SOLID_AUTHORED_BINDING_PATH_CAPACITY];
    char material_digest_sha256[
        PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
} ProceduralSolidAuthoredMaterialReferenceV1;

typedef struct ProceduralSolidAuthoredMaterialBindingV1 {
    uint32_t schema_version;
    char binding_id[64];
    char region_binding_id[64];
    char region_binding_digest_sha256[
        PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
    size_t assignment_count;
    ProceduralSolidAuthoredMaterialReferenceV1
        assignments[PROCEDURAL_SOLID_REGION_MAX];
} ProceduralSolidAuthoredMaterialBindingV1;

typedef enum ProceduralSolidAuthoredBindingStatus {
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_OK = 0,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_ARGUMENT,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_IO,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_JSON,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_SCHEMA,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_IDENTITY,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_REGION,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_MATERIAL,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_STALE_BASE,
    PROCEDURAL_SOLID_AUTHORED_BINDING_STATUS_CAPACITY
} ProceduralSolidAuthoredBindingStatus;

typedef struct ProceduralSolidAuthoredBindingReport {
    ProceduralSolidAuthoredBindingStatus status;
    char field[96];
    char message[256];
    char binding_digest_sha256[
        PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY];
} ProceduralSolidAuthoredBindingReport;

void ProceduralSolidAuthoredMaterialBindingV1_Init(
    ProceduralSolidAuthoredMaterialBindingV1 *binding);

bool ProceduralSolidAuthoredMaterialBindingV1_FromRegionBinding(
    const char *binding_id,
    const ProceduralSolidMaterialBindingV1 *region_binding,
    ProceduralSolidAuthoredMaterialBindingV1 *out_binding,
    ProceduralSolidAuthoredBindingReport *report);

bool ProceduralSolidAuthoredMaterialBindingV1_LoadJsonFile(
    const char *path,
    ProceduralSolidAuthoredMaterialBindingV1 *out_binding,
    ProceduralSolidAuthoredBindingReport *report);

bool ProceduralSolidAuthoredMaterialBindingV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    ProceduralSolidAuthoredBindingReport *report);

bool ProceduralSolidAuthoredMaterialBindingV1_Validate(
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    const ProceduralSolidMaterialBindingV1 *region_binding,
    ProceduralSolidAuthoredBindingReport *report);

bool ProceduralSolidAuthoredMaterialBindingV1_Digest(
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    char out_digest[PROCEDURAL_SOLID_MATERIAL_BINDING_DIGEST_CAPACITY],
    ProceduralSolidAuthoredBindingReport *report);

bool ProceduralSolidAuthoredMaterialBindingV1_AssignRegion(
    const ProceduralSolidAuthoredMaterialBindingV1 *base,
    const char *expected_base_digest,
    const char *region_id,
    const char *material_path,
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredMaterialBindingV1 *out_binding,
    ProceduralSolidAuthoredBindingReport *report);

bool ProceduralSolidAuthoredMaterialBindingV1_AssignKind(
    const ProceduralSolidAuthoredMaterialBindingV1 *base,
    const char *expected_base_digest,
    ProceduralSolidRegionKind kind,
    const ProceduralSolidMaterialBindingV1 *region_binding,
    const char *material_path,
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredMaterialBindingV1 *out_binding,
    ProceduralSolidAuthoredBindingReport *report);

const ProceduralSolidAuthoredMaterialReferenceV1 *
ProceduralSolidAuthoredMaterialBindingV1_Resolve(
    const ProceduralSolidAuthoredMaterialBindingV1 *binding,
    const char *region_id);

#endif
