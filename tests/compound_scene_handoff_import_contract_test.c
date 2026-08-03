#include "import/compound_scene_binding_manifest.h"
#include "import/compound_scene_handoff_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "compound scene handoff import contract failed " \
        "line=%d check=%s\n", __LINE__, #condition); return 1; \
} } while (0)

static uint64_t hash_bytes(uint64_t hash, const void* data, size_t size) {
    const unsigned char* bytes = data;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    unsigned char bytes[8];
    for (size_t i = 0; i < sizeof(bytes); ++i)
        bytes[i] = (unsigned char)(value >> (8u * i));
    return hash_bytes(hash, bytes, sizeof(bytes));
}

static char* read_text(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file)
        return NULL;
    char* text = malloc(RAY_COMPOUND_SCENE_HANDOFF_TEXT_CAPACITY);
    if (!text) {
        fclose(file);
        return NULL;
    }
    const size_t size = fread(text, 1,
        RAY_COMPOUND_SCENE_HANDOFF_TEXT_CAPACITY - 1u, file);
    const bool ok = !ferror(file) && feof(file) && fclose(file) == 0;
    if (!ok) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

static void populate_renderer_bindings(
    RayCompoundSceneBindingManifest* manifest) {
    snprintf(manifest->bindings[0].object_id,
        sizeof(manifest->bindings[0].object_id), "%s", "sim_body_c2");
    snprintf(manifest->bindings[0].mesh_asset_id,
        sizeof(manifest->bindings[0].mesh_asset_id), "%s", "mesh_c2_u_channel");
    snprintf(manifest->bindings[1].object_id,
        sizeof(manifest->bindings[1].object_id), "%s", "sim_body_c1");
    snprintf(manifest->bindings[1].mesh_asset_id,
        sizeof(manifest->bindings[1].mesh_asset_id), "%s", "mesh_c1_l_bracket");
}

static int check_import_and_replay(const char* fixture_path) {
    RayCompoundSceneHandoff handoff;
    RayCompoundSceneImportFailure failure;
    ray_compound_scene_handoff_init(&handoff);
    CHECK(ray_compound_scene_handoff_read(fixture_path, &handoff, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_IMPORT_FAILURE_NONE);
    CHECK(ray_compound_scene_handoff_validate(&handoff));
    CHECK(handoff.fixture_digest == UINT64_C(0x4c1eb5781cb35a5a));
    CHECK(handoff.handoff_digest == UINT64_C(0x51a8af28de622ad8));
    CHECK(handoff.seed == UINT64_C(0x2026072700410001));
    CHECK(handoff.fixed_dt_s > 0.0041666666666);
    CHECK(handoff.fixed_dt_s < 0.0041666666667);
    CHECK(handoff.frame_count == 721u);
    CHECK(!strcmp(handoff.bindings[0].source_asset_id, "c2_u_channel_v1"));
    CHECK(!strcmp(handoff.bindings[1].source_asset_id, "c1_l_bracket_v1"));
    CHECK(handoff.bindings[0].binding_digest ==
        UINT64_C(0x8564a83bf30cc84e));
    CHECK(handoff.bindings[1].binding_digest ==
        UINT64_C(0x4936ad29d47f87a3));

    static const uint64_t ticks[] = {0, 240, 480, 720};
    uint64_t replay_digest = UINT64_C(1469598103934665603);
    replay_digest = hash_u64(replay_digest, handoff.handoff_digest);
    for (size_t i = 0; i < sizeof(ticks) / sizeof(ticks[0]); ++i) {
        RayCompoundSceneFrame frame;
        CHECK(ray_compound_scene_handoff_replay_exact(
            &handoff, ticks[i], &frame));
        CHECK(frame.tick == ticks[i]);
        CHECK(frame.bodies[0].body_id == handoff.bindings[0].body_id);
        CHECK(frame.bodies[1].body_id == handoff.bindings[1].body_id);
        replay_digest = hash_u64(replay_digest, frame.tick);
        replay_digest = hash_bytes(replay_digest, frame.bodies,
            sizeof(frame.bodies));
    }
    RayCompoundSceneFrame rejected;
    CHECK(!ray_compound_scene_handoff_replay_exact(
        &handoff, handoff.frame_count, &rejected));
    CHECK(replay_digest == UINT64_C(0xad3d5f42d9b0df03));

    RayCompoundSceneBindingManifest manifest;
    ray_compound_scene_binding_manifest_init(&manifest, &handoff);
    CHECK(!ray_compound_scene_binding_manifest_validate(&manifest, &handoff));
    populate_renderer_bindings(&manifest);
    CHECK(ray_compound_scene_binding_manifest_validate(&manifest, &handoff));
    CHECK(ray_compound_scene_binding_manifest_find_body(
        &manifest, handoff.bindings[0].body_id) == &manifest.bindings[0]);
    CHECK(ray_compound_scene_binding_manifest_find_body(&manifest, 999) == NULL);

    RayCompoundSceneBindingManifest rejected_manifest = manifest;
    rejected_manifest.handoff_digest ^= UINT64_C(1);
    CHECK(!ray_compound_scene_binding_manifest_validate(
        &rejected_manifest, &handoff));
    rejected_manifest = manifest;
    snprintf(rejected_manifest.bindings[1].object_id,
        sizeof(rejected_manifest.bindings[1].object_id), "%s",
        rejected_manifest.bindings[0].object_id);
    CHECK(!ray_compound_scene_binding_manifest_validate(
        &rejected_manifest, &handoff));
    rejected_manifest = manifest;
    rejected_manifest.world_from_simulation_translation_m.x = 1.0;
    CHECK(!ray_compound_scene_binding_manifest_validate(
        &rejected_manifest, &handoff));
    rejected_manifest = manifest;
    snprintf(rejected_manifest.playback,
        sizeof(rejected_manifest.playback), "%s", "interpolated");
    CHECK(!ray_compound_scene_binding_manifest_validate(
        &rejected_manifest, &handoff));
    rejected_manifest = manifest;
    rejected_manifest.bindings[1].source_sha256[0] =
        rejected_manifest.bindings[1].source_sha256[0] == '0' ? '1' : '0';
    CHECK(!ray_compound_scene_binding_manifest_validate(
        &rejected_manifest, &handoff));

    ray_compound_scene_handoff_free(&handoff);
    return 0;
}

static int check_envelope_rejections(const char* fixture_path) {
    char* canonical = read_text(fixture_path);
    CHECK(canonical != NULL);
    const size_t size = strlen(canonical);
    CHECK(size > 128u);

    RayCompoundSceneHandoff rejected;
    RayCompoundSceneImportFailure failure;
    ray_compound_scene_handoff_init(&rejected);

    char* tampered = malloc(size + 2u);
    CHECK(tampered != NULL);
    memcpy(tampered, canonical, size + 1u);
    char* payload = strstr(tampered, "payload_hex=");
    CHECK(payload != NULL);
    payload += strlen("payload_hex=");
    payload[20] = payload[20] == '0' ? '1' : '0';
    CHECK(!ray_compound_scene_handoff_parse(tampered, &rejected, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);

    memcpy(tampered, canonical, size + 1u);
    payload = strstr(tampered, "payload_hex=") + strlen("payload_hex=");
    payload[0] = 'A';
    CHECK(!ray_compound_scene_handoff_parse(tampered, &rejected, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);

    memcpy(tampered, canonical, size + 1u);
    tampered[size] = 'x';
    tampered[size + 1u] = '\0';
    CHECK(!ray_compound_scene_handoff_parse(tampered, &rejected, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);

    CHECK(!ray_compound_scene_handoff_parse(
        "not_a_compound_scene_packet\n", &rejected, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_IMPORT_FAILURE_ENVELOPE);
    CHECK(!ray_compound_scene_handoff_read(
        "tests/fixtures/compound_scene_handoff/does_not_exist.txt",
        &rejected, &failure));
    CHECK(failure == RAY_COMPOUND_SCENE_IMPORT_FAILURE_IO);

    free(tampered);
    free(canonical);
    return 0;
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    CHECK(sizeof(RayCompoundSceneBodyTransform) == 64u);
    CHECK(check_import_and_replay(argv[1]) == 0);
    CHECK(check_envelope_rejections(argv[1]) == 0);
    puts("compound scene handoff import contract: PASS "
         "handoff=51a8af28de622ad8 replay=ad3d5f42d9b0df03 "
         "bindings=2 frames=721 composition=replace_world_rigid "
         "playback=exact_step world_from_simulation=identity");
    return 0;
}
