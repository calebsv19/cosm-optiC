#include "procedural/procedural_surface_authoring_document.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void set_ref(ProceduralSurfaceAuthoringDocumentRef *ref,
                    const char *id, uint32_t domains, char digit) {
    snprintf(ref->id, sizeof(ref->id), "%s", id);
    memset(ref->digest_sha256, digit, 64u);
    ref->digest_sha256[64] = '\0';
    ref->output_domains = domains;
}

static void test_document_contract(void) {
    ProceduralSurfaceAuthoringDocumentV1 document;
    ProceduralSurfaceAuthoringDocumentV1 reordered;
    ProceduralSurfaceAuthoringDocumentReport report;
    char canonical[16384];
    char digest[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    char digest_repeat[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    ProceduralSurfaceAuthoringDocumentCompilePlan plan;
    ProceduralSurfaceAuthoringDocumentV1_Init(&document);
    snprintf(document.document_id, sizeof(document.document_id), "cube_surface_v1");
    snprintf(document.source_object_id, sizeof(document.source_object_id), "cube");
    memset(document.source_mesh_digest_sha256, 'a', 64u);
    document.source_mesh_digest_sha256[64] = '\0';
    set_ref(&document.material_graph, "wood_material",
            PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MATERIAL |
                PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MICRODETAIL_NORMAL,
            'b');
    set_ref(&document.surface_field_graph, "dirt_field",
            PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_MICRODETAIL_NORMAL,
            'c');
    set_ref(&document.face_region_selector, "top_face",
            PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET,
            'd');
    document.attachment_count = 1u;
    set_ref(&document.attachments[0], "grass_asset",
            PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET, 'e');
    assert(ProceduralSurfaceAuthoringDocumentV1_Validate(&document, &report));
    assert(ProceduralSurfaceAuthoringDocumentV1_CanonicalJson(
        &document, canonical, sizeof(canonical), &report));
    assert(strstr(canonical, "surface_field_graph") != NULL);
    assert(ProceduralSurfaceAuthoringDocumentV1_Digest(
        &document, digest, &report));
    reordered = document;
    reordered.attachment_count = 1u;
    assert(ProceduralSurfaceAuthoringDocumentV1_Digest(
        &reordered, digest_repeat, &report));
    assert(strcmp(digest, digest_repeat) == 0);
    assert(ProceduralSurfaceAuthoringDocumentV1_Compile(
        &document, &plan, &report));
    assert(plan.valid);
    assert(strcmp(plan.document_digest_sha256, digest) == 0);
    assert(plan.attachment_count == 1u);
    assert((plan.output_domains &
            PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET) != 0u);
    reordered.surface_field_graph.output_domains =
        PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_OUTPUT_ATTACHED_ASSET;
    assert(!ProceduralSurfaceAuthoringDocumentV1_Validate(&reordered, &report));
    assert(report.status ==
           PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DOMAIN);
    printf("authoring_document digest=%s canonical_bytes=%zu\n", digest,
           strlen(canonical));
}

static void test_json_round_trip(void) {
    const char *path = "/tmp/procedural_surface_authoring_document_v1.json";
    FILE *file = fopen(path, "w");
    ProceduralSurfaceAuthoringDocumentV1 loaded;
    ProceduralSurfaceAuthoringDocumentReport report;
    assert(file != NULL);
    fputs("{\"schema\":\"ray_tracing.surface_authoring_document\","
          "\"schema_version\":1,\"document_id\":\"cube\","
          "\"source_object_id\":\"cube\","
          "\"source_mesh_digest_sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
          "\"material_graph\":null,\"surface_field_graph\":null,"
          "\"face_region_selector\":null,\"attachments\":["
          "{\"id\":\"grass\",\"digest_sha256\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\",\"output_domains\":16}]}\n",
          file);
    fclose(file);
    assert(ProceduralSurfaceAuthoringDocumentV1_LoadJsonFile(
        path, &loaded, &report));
    assert(loaded.attachment_count == 1u);
    unlink(path);
}

static void test_focused_validation_failures(void) {
    ProceduralSurfaceAuthoringDocumentV1 document;
    ProceduralSurfaceAuthoringDocumentReport report;
    ProceduralSurfaceAuthoringDocumentV1_Init(&document);
    snprintf(document.document_id, sizeof(document.document_id), "cube");
    snprintf(document.source_object_id, sizeof(document.source_object_id), "cube");
    memset(document.source_mesh_digest_sha256, 'a', 64u);
    document.source_mesh_digest_sha256[64] = '\0';
    set_ref(&document.material_graph, "material", 1u, 'b');
    document.attachment_count = 2u;
    set_ref(&document.attachments[0], "grass", 16u, 'c');
    set_ref(&document.attachments[1], "grass", 16u, 'd');
    assert(!ProceduralSurfaceAuthoringDocumentV1_Validate(&document, &report));
    assert(report.status == PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_IDENTITY);
    document.attachment_count = 1u;
    document.material_graph.output_domains = 16u;
    assert(!ProceduralSurfaceAuthoringDocumentV1_Validate(&document, &report));
    assert(report.status == PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DOMAIN);
    document.material_graph.output_domains = 1u;
    memset(&document.material_graph, 0, sizeof(document.material_graph));
    document.attachment_count = 0u;
    assert(!ProceduralSurfaceAuthoringDocumentV1_Validate(&document, &report));
    assert(report.status == PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_SCHEMA);
}

static void test_transactional_edit_save_readback(void) {
    const char *path = "/tmp/procedural_surface_authoring_document_edit.json";
    ProceduralSurfaceAuthoringDocumentV1 document;
    ProceduralSurfaceAuthoringDocumentV1 edited;
    ProceduralSurfaceAuthoringDocumentV1 undo;
    ProceduralSurfaceAuthoringDocumentV1 readback;
    ProceduralSurfaceAuthoringDocumentRef replacement;
    ProceduralSurfaceAuthoringDocumentCompilePlan plan;
    ProceduralSurfaceAuthoringDocumentReport report;
    char digest[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];
    char replacement_digest[PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_DIGEST_CAPACITY];

    ProceduralSurfaceAuthoringDocumentV1_Init(&document);
    snprintf(document.document_id, sizeof(document.document_id), "cube_edit");
    snprintf(document.source_object_id, sizeof(document.source_object_id), "cube");
    memset(document.source_mesh_digest_sha256, 'a', 64u);
    document.source_mesh_digest_sha256[64] = '\0';
    set_ref(&document.material_graph, "brown_mix", 1u, 'b');
    assert(ProceduralSurfaceAuthoringDocumentV1_Digest(&document, digest, &report));
    set_ref(&replacement, "umber_mix", 1u, 'f');
    assert(ProceduralSurfaceAuthoringDocumentV1_ReplaceReferenceTransactional(
        &document, "material_graph", &replacement, digest,
        document.source_mesh_digest_sha256, document.material_graph.digest_sha256,
        &edited, &undo, &report));
    assert(strcmp(undo.material_graph.id, "brown_mix") == 0);
    assert(strcmp(edited.material_graph.id, "umber_mix") == 0);
    assert(!ProceduralSurfaceAuthoringDocumentV1_ReplaceReferenceTransactional(
        &document, "material_graph", &replacement,
        "0000000000000000000000000000000000000000000000000000000000000000",
        document.source_mesh_digest_sha256, document.material_graph.digest_sha256,
        &edited, &undo, &report));
    assert(report.status == PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_STATUS_DIGEST);
    assert(ProceduralSurfaceAuthoringDocumentV1_SaveJsonFileAtomic(path, &edited, &report));
    assert(ProceduralSurfaceAuthoringDocumentV1_ReadbackJsonFile(
        path, &readback, &plan, &report));
    assert(plan.valid);
    assert(strcmp(readback.material_graph.id, "umber_mix") == 0);
    assert(ProceduralSurfaceAuthoringDocumentV1_Digest(
        &edited, replacement_digest, &report));
    assert(strcmp(plan.document_digest_sha256, replacement_digest) == 0);
    unlink(path);
    printf("authoring_document transaction=ok save=ok readback=ok\n");
}

int main(void) {
    test_document_contract();
    test_json_round_trip();
    test_focused_validation_failures();
    test_transactional_edit_save_readback();
    return 0;
}
