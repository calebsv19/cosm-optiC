#ifndef PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_H
#define PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_SCHEMA \
    "ray_tracing.surface_authoring_document"
#define PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_SCHEMA_VERSION 1u
#define PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_ID_CAPACITY 64u
#define PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY 65u
#define PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_REF_CAPACITY 64u
#define PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_MAX_ATTACHMENTS 8u

typedef enum ProceduralSurfaceAuthoringDocumentOutputDomain {
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_INVALID = 0,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MATERIAL = 1u << 0,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MICRODETAIL_NORMAL = 1u << 1,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_SIGNED_RELIEF = 1u << 2,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_DEEP_INSET = 1u << 3,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET = 1u << 4
} ProceduralSurfaceAuthoringDocumentOutputDomain;

typedef struct ProceduralSurfaceAuthoringDocumentRef {
    char id[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_REF_CAPACITY];
    char digest_sha256[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    uint32_t output_domains;
} ProceduralSurfaceAuthoringDocumentRef;

typedef struct ProceduralSurfaceAuthoringDocumentV1 {
    uint32_t schema_version;
    char document_id[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_ID_CAPACITY];
    char source_object_id[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_REF_CAPACITY];
    char source_mesh_digest_sha256[
        PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    ProceduralSurfaceAuthoringDocumentRef material_graph;
    ProceduralSurfaceAuthoringDocumentRef surface_field_graph;
    ProceduralSurfaceAuthoringDocumentRef face_region_selector;
    size_t attachment_count;
    ProceduralSurfaceAuthoringDocumentRef
        attachments[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_MAX_ATTACHMENTS];
} ProceduralSurfaceAuthoringDocumentV1;

typedef struct ProceduralSurfaceAuthoringDocumentCompilePlan {
    bool valid;
    char document_id[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_ID_CAPACITY];
    char document_digest_sha256[
        PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    char source_object_id[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_REF_CAPACITY];
    char source_mesh_digest_sha256[
        PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    uint32_t output_domains;
    uint32_t attachment_count;
    bool material_graph_bound;
    bool surface_field_graph_bound;
    bool face_region_selector_bound;
} ProceduralSurfaceAuthoringDocumentCompilePlan;

typedef enum ProceduralSurfaceAuthoringDocumentStatus {
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_OK = 0,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_NULL_ARGUMENT,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IO,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_JSON,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_SCHEMA,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IDENTITY,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DOMAIN,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CAPACITY,
    PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_CANONICALIZATION
} ProceduralSurfaceAuthoringDocumentStatus;

typedef struct ProceduralSurfaceAuthoringDocumentReport {
    ProceduralSurfaceAuthoringDocumentStatus status;
    char field[96];
    char message[256];
} ProceduralSurfaceAuthoringDocumentReport;

void ProceduralSurfaceAuthoringDocumentV1_Init(
    ProceduralSurfaceAuthoringDocumentV1 *document);

bool ProceduralSurfaceAuthoringDocumentV1_Validate(
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    ProceduralSurfaceAuthoringDocumentReport *report);

bool ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(
    const char *path,
    ProceduralSurfaceAuthoringDocumentV1 *out_document,
    ProceduralSurfaceAuthoringDocumentReport *report);

bool ProceduralSurfaceAuthoringDocumentV1_CanonicalJson(
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    char *out_json,
    size_t out_capacity,
    ProceduralSurfaceAuthoringDocumentReport *report);

bool ProceduralSurfaceAuthoringDocumentV1_Digest(
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    char out_digest[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY],
    ProceduralSurfaceAuthoringDocumentReport *report);

bool ProceduralSurfaceAuthoringDocumentV1_Compile(
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    ProceduralSurfaceAuthoringDocumentCompilePlan *out_plan,
    ProceduralSurfaceAuthoringDocumentReport *report);

/* Apply one digest-guarded reference replacement without mutating the input.
   out_undo receives the exact pre-edit document for local undo/readback. */
bool ProceduralSurfaceAuthoringDocumentV1_ReplaceReferenceTransactional(
    const ProceduralSurfaceAuthoringDocumentV1 *base_document,
    const char *field,
    const ProceduralSurfaceAuthoringDocumentRef *replacement,
    const char *expected_document_digest,
    const char *expected_source_mesh_digest,
    const char *expected_reference_digest,
    ProceduralSurfaceAuthoringDocumentV1 *out_document,
    ProceduralSurfaceAuthoringDocumentV1 *out_undo,
    ProceduralSurfaceAuthoringDocumentReport *report);

bool ProceduralSurfaceAuthoringDocumentV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralSurfaceAuthoringDocumentV1 *document,
    ProceduralSurfaceAuthoringDocumentReport *report);

bool ProceduralSurfaceAuthoringDocumentV1_ReadbackJsonFile(
    const char *path,
    ProceduralSurfaceAuthoringDocumentV1 *out_document,
    ProceduralSurfaceAuthoringDocumentCompilePlan *out_plan,
    ProceduralSurfaceAuthoringDocumentReport *report);

const char *ProceduralSurfaceAuthoringDocumentStatus_Name(
    ProceduralSurfaceAuthoringDocumentStatus status);

#endif
