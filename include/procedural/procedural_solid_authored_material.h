#ifndef PROCEDURAL_SOLID_AUTHORED_MATERIAL_H
#define PROCEDURAL_SOLID_AUTHORED_MATERIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SOLID_AUTHORED_MATERIAL_SCHEMA \
    "ray_tracing.procedural_solid_authored_material"
#define PROCEDURAL_SOLID_AUTHORED_MATERIAL_SCHEMA_VERSION 1u
#define PROCEDURAL_SOLID_AUTHORED_MATERIAL_ID_CAPACITY 64u
#define PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY 65u
#define PROCEDURAL_SOLID_AUTHORED_MATERIAL_KIND_CAPACITY 32u

typedef struct ProceduralSolidAuthoredTextureV1 {
    bool enabled;
    char kind[PROCEDURAL_SOLID_AUTHORED_MATERIAL_KIND_CAPACITY];
    double scale_units;
    double strength;
    double coverage;
    double grain;
    double edge_softness;
    double contrast;
    double flow;
    double color_depth;
    double surface_damage;
    double microdetail_normal_strength;
    int seed;
} ProceduralSolidAuthoredTextureV1;

typedef struct ProceduralSolidAuthoredMaterialSurfaceV1 {
    double base_color_r;
    double base_color_g;
    double base_color_b;
    double roughness;
    double metallic;
    double reflectivity;
    double specular;
    double emission_color_r;
    double emission_color_g;
    double emission_color_b;
    double emission_strength;
    double transparency;
    double ior;
    ProceduralSolidAuthoredTextureV1 texture;
} ProceduralSolidAuthoredMaterialSurfaceV1;

typedef struct ProceduralSolidAuthoredMaterialV1 {
    uint32_t schema_version;
    char material_id[PROCEDURAL_SOLID_AUTHORED_MATERIAL_ID_CAPACITY];
    char template_id[PROCEDURAL_SOLID_AUTHORED_MATERIAL_ID_CAPACITY];
    ProceduralSolidAuthoredMaterialSurfaceV1 surface;
} ProceduralSolidAuthoredMaterialV1;

typedef enum ProceduralSolidAuthoredMaterialStatus {
    PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_OK = 0,
    PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_ARGUMENT,
    PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_IO,
    PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_JSON,
    PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_SCHEMA,
    PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_IDENTITY,
    PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_PARAMETER,
    PROCEDURAL_SOLID_AUTHORED_MATERIAL_STATUS_STALE_BASE
} ProceduralSolidAuthoredMaterialStatus;

typedef struct ProceduralSolidAuthoredMaterialReport {
    ProceduralSolidAuthoredMaterialStatus status;
    char field[96];
    char message[256];
    char material_digest_sha256[
        PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY];
} ProceduralSolidAuthoredMaterialReport;

void ProceduralSolidAuthoredMaterialV1_Init(
    ProceduralSolidAuthoredMaterialV1 *material);

bool ProceduralSolidAuthoredMaterialV1_FromTemplate(
    const char *template_id,
    const char *material_id,
    ProceduralSolidAuthoredMaterialV1 *out_material,
    ProceduralSolidAuthoredMaterialReport *report);

bool ProceduralSolidAuthoredMaterialV1_LoadJsonFile(
    const char *path,
    ProceduralSolidAuthoredMaterialV1 *out_material,
    ProceduralSolidAuthoredMaterialReport *report);

bool ProceduralSolidAuthoredMaterialV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredMaterialReport *report);

bool ProceduralSolidAuthoredMaterialV1_Validate(
    const ProceduralSolidAuthoredMaterialV1 *material,
    ProceduralSolidAuthoredMaterialReport *report);

bool ProceduralSolidAuthoredMaterialV1_Digest(
    const ProceduralSolidAuthoredMaterialV1 *material,
    char out_digest[PROCEDURAL_SOLID_AUTHORED_MATERIAL_DIGEST_CAPACITY],
    ProceduralSolidAuthoredMaterialReport *report);

bool ProceduralSolidAuthoredMaterialV1_SetParameter(
    const ProceduralSolidAuthoredMaterialV1 *base,
    const char *expected_base_digest,
    const char *parameter_id,
    const char *value,
    ProceduralSolidAuthoredMaterialV1 *out_material,
    ProceduralSolidAuthoredMaterialReport *report);

const char *ProceduralSolidAuthoredMaterialStatus_Name(
    ProceduralSolidAuthoredMaterialStatus status);

#endif
