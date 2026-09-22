#include "app/ray_tracing_sha256.h"
/* Native failure-injection acceptance: retained source and runtime generations
 * have distinct lifetimes, and every late failure clears the entire runtime. */
static void lifecycle_t0_empty(void) {
    assert(runtime_scene_bridge_empty_after_failure());
    assert(sceneSettings.objectCount == 0);
    assert(!animSettings.runtimeScenePath[0]);
    assert(RuntimeSurfaceSamplingPreparedBytes() == 0);
    for (int i = 0; i < MAX_OBJECTS; ++i) {
        char id[64] = {0};
        assert(!runtime_scene_bridge_get_last_object_id_for_scene_index(i, id, sizeof(id)));
        assert(!RuntimeSurfaceMappingActive(i));
        assert(!RuntimeSurfaceSamplingActive(i));
        assert(!RuntimeSurfaceGraphActive(i));
    }
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
    RuntimeSceneBridge3DDigestState digest = {0};
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    RuntimeSceneBridge3DLightSeedState lights = {0};
    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    assert(!scaffold.valid && !digest.valid && !seeds.valid && !lights.valid);
    assert(seeds.primitive_count == 0 && lights.light_count == 0);
    const RayTracingRuntimeMeshAssetSet *assets = ray_tracing_runtime_mesh_assets_last();
    assert(!assets ||
           (!assets->asset_count && !assets->instance_count && !assets->skipped_instance_count));
}

static void lifecycle_t0_recover(const char *graph_scene) {
    RuntimeSceneBridgePreflight summary = {0};
    assert(runtime_scene_bridge_apply_file(graph_scene, &summary));
    assert(!runtime_scene_bridge_empty_after_failure());
    assert(sceneSettings.objectCount > 0 && RuntimeSurfaceGraphActive(0));
    char id[64] = {0};
    assert(runtime_scene_bridge_get_last_object_id_for_scene_index(0, id, sizeof(id)));
    assert(id[0]);
}

static void lifecycle_t0_transform(const SceneEditorDocumentTransform *expected) {
    SceneEditorDocumentTransform actual;
    char diagnostic[512];
    assert(
        SceneEditorDocumentGetTransformForSceneIndex(0, &actual, diagnostic, sizeof(diagnostic)));
    for (int a = 0; a < 3; ++a) {
        assert(actual.position[a] == expected->position[a]);
        assert(actual.rotation_degrees[a] == expected->rotation_degrees[a]);
        assert(actual.scale[a] == expected->scale[a]);
    }
}

static void surface_lifecycle_t0_probe(const char *graph_scene) {
    const char *sampling_scene = getenv("OPTIC_T0_SAMPLING_SCENE");
    assert(sampling_scene && sampling_scene[0]);
    assert(RuntimeSurfaceGraphActive(0));
    RuntimeSceneBridgePreflight summary = {0};
    int original_count = sceneSettings.objectCount;
    char id[64] = {0}, after_id[64] = {0};
    assert(runtime_scene_bridge_get_last_object_id_for_scene_index(0, id, sizeof(id)));
    unsigned long long generation = RuntimeSurfaceMappingRevision();
    int asset_count = ray_tracing_runtime_mesh_assets_last()->asset_count;
    assert(asset_count > 0);
    assert(!runtime_scene_bridge_apply_json("{", &summary));
    assert(!runtime_scene_bridge_apply_file("missing-t0-scene-does-not-exist.json", &summary));
    assert(!runtime_scene_bridge_empty_after_failure());
    assert(RuntimeSurfaceGraphActive(0) && sceneSettings.objectCount == original_count);
    assert(RuntimeSurfaceMappingRevision() == generation);
    assert(ray_tracing_runtime_mesh_assets_last()->asset_count == asset_count);
    assert(runtime_scene_bridge_get_last_object_id_for_scene_index(0, after_id, sizeof(after_id)));
    assert(!strcmp(id, after_id));

    /* The material staging API itself preserves the previous published tables. */
    json_object *candidate = json_object_from_file(sampling_scene);
    assert(candidate);
    RuntimeSurfacePreparationFailNextForTests(RUNTIME_SURFACE_FAIL_ALLOCATION);
    assert(!RuntimeSurfaceMappingLoadScene(candidate, 1));
    assert(RuntimeSurfaceGraphActive(0) && RuntimeSurfaceMappingRevision() == generation);
    json_object_put(candidate);

    const RuntimeSurfacePreparationFailure failures[] = {RUNTIME_SURFACE_FAIL_ALLOCATION,
                                                         RUNTIME_SURFACE_FAIL_IMAGE_READ,
                                                         RUNTIME_SURFACE_FAIL_IMAGE_DECODE,
                                                         RUNTIME_SURFACE_FAIL_PYRAMID_ALLOCATION};
    for (size_t f = 0; f < sizeof(failures) / sizeof(failures[0]); ++f) {
        RuntimeSurfacePreparationFailNextForTests(failures[f]);
        assert(!runtime_scene_bridge_apply_file(sampling_scene, &summary));
        assert(strstr(summary.diagnostics, "preparation"));
        lifecycle_t0_empty();
        /* Reject a file after a previous late clear: no rejected path may leak. */
        json_object *bad = json_object_from_file(graph_scene);
        assert(bad);
        json_object_object_add(bad, "world_scale", json_object_new_double(0));
        assert(json_object_to_file("invalid-after-clear.json", bad) == 0);
        json_object_put(bad);
        assert(!runtime_scene_bridge_apply_file("invalid-after-clear.json", &summary));
        lifecycle_t0_empty();
        lifecycle_t0_recover(graph_scene);
    }

    char diagnostic[1024] = {0};
    SceneEditorDocumentTransform before, changed;
    assert(
        SceneEditorDocumentGetTransformForSceneIndex(0, &before, diagnostic, sizeof(diagnostic)));
    changed = before;
    changed.position[0] += .125;
    /* The general transform command only accepts positive scale; restore must
     * nevertheless preserve the originally imported mirrored transform. */
    for (int a = 0; a < 3; ++a) changed.scale[a] = fabs(changed.scale[a]);
    unsigned long long revision = SceneEditorDocumentRevision();
    int document_count = SceneEditorDocumentObjectCount();
    bool dirty = SceneEditorDocumentIsDirty();
    bool undo = SceneEditorDocumentCanUndo(), redo = SceneEditorDocumentCanRedo();

    SceneEditorDocumentFailNextForTests(SCENE_DOCUMENT_FAIL_SNAPSHOT);
    assert(
        !SceneEditorDocumentSetTransformForSceneIndex(0, &changed, diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "snapshot"));
    assert(RuntimeSurfaceGraphActive(0) && !runtime_scene_bridge_empty_after_failure());
    lifecycle_t0_transform(&before);
    assert(SceneEditorDocumentRevision() == revision && SceneEditorDocumentIsDirty() == dirty);
    assert(SceneEditorDocumentCanUndo() == undo && SceneEditorDocumentCanRedo() == redo);

    SceneEditorDocumentFailNextForTests(SCENE_DOCUMENT_FAIL_HISTORY);
    assert(
        !SceneEditorDocumentSetTransformForSceneIndex(0, &changed, diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "history"));
    assert(RuntimeSurfaceGraphActive(0) && !runtime_scene_bridge_empty_after_failure());
    lifecycle_t0_transform(&before);
    assert(SceneEditorDocumentRevision() == revision && SceneEditorDocumentIsDirty() == dirty);
    assert(SceneEditorDocumentCanUndo() == undo && SceneEditorDocumentCanRedo() == redo);

    /* Candidate adoption reserves history before changing disk or runtime. */
    json_object *adopt = json_object_from_file(graph_scene);
    assert(adopt);
    json_object *objects = NULL, *transform = NULL, *position = NULL;
    assert(json_object_object_get_ex(adopt, "objects", &objects));
    assert(json_object_object_get_ex(json_object_array_get_idx(objects, 0), "transform", &transform));
    assert(json_object_object_get_ex(transform, "position", &position));
    json_object_object_add(position, "x", json_object_new_double(before.position[0] + .25));
    assert(json_object_to_file("adopt-candidate.json", adopt) == 0);
    json_object_put(adopt);
    char candidate_path[PATH_MAX];
    assert(getcwd(candidate_path, sizeof(candidate_path)));
    strncat(candidate_path, "/adopt-candidate.json", sizeof(candidate_path)-strlen(candidate_path)-1);
    char disk_before[65], disk_after[65];
    assert(ray_tracing_sha256_file(graph_scene, disk_before));
    SceneEditorDocumentFailNextForTests(SCENE_DOCUMENT_FAIL_HISTORY);
    assert(!SceneEditorDocumentAdoptCandidateAsCommand(candidate_path, diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "history"));
    assert(ray_tracing_sha256_file(graph_scene, disk_after));
    assert(!strcmp(disk_before, disk_after));
    lifecycle_t0_transform(&before);
    assert(RuntimeSurfaceGraphActive(0) && SceneEditorDocumentRevision() == revision);
    /* A following failed ordinary command proves no pending command was leaked. */
    SceneEditorDocumentFailNextForTests(SCENE_DOCUMENT_FAIL_HISTORY);
    assert(!SceneEditorDocumentSetTransformForSceneIndex(0, &changed, diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "history"));
    lifecycle_t0_transform(&before);

    RuntimeSurfacePreparationFailNextForTests(RUNTIME_SURFACE_FAIL_ALLOCATION);
    SceneEditorDocumentFailNextForTests(SCENE_DOCUMENT_FAIL_RESTORE);
    assert(
        !SceneEditorDocumentSetTransformForSceneIndex(0, &changed, diagnostic, sizeof(diagnostic)));
    fprintf(stderr, "T0 restore diagnostic: %s\n", diagnostic);
    assert(strstr(diagnostic, "restore failed") && strstr(diagnostic, "runtime scene cleared"));
    lifecycle_t0_empty();
    assert(SceneEditorDocumentIsOpen() && SceneEditorDocumentObjectCount() == document_count);
    assert(SceneEditorDocumentRevision() == revision && SceneEditorDocumentIsDirty() == dirty);
    assert(SceneEditorDocumentCanUndo() == undo && SceneEditorDocumentCanRedo() == redo);
    /* Restore only runtime IDs/assets. Do not reopen the document: this readback
     * must prove the retained rollback state, not reload it from disk. */
    lifecycle_t0_recover(graph_scene);
    lifecycle_t0_transform(&before);
    assert(SceneEditorDocumentRevision() == revision);

    RuntimeSurfacePreparationFailNextForTests(RUNTIME_SURFACE_FAIL_ALLOCATION);
    assert(!SceneEditorDocumentAdoptCandidateAsCommand(candidate_path, diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "preparation"));
    assert(ray_tracing_sha256_file(graph_scene, disk_after));
    assert(!strcmp(disk_before, disk_after));
    lifecycle_t0_transform(&before);
    assert(RuntimeSurfaceGraphActive(0) && SceneEditorDocumentRevision() == revision);
    assert(SceneEditorDocumentAdoptCandidateAsCommand(candidate_path, diagnostic, sizeof(diagnostic)));
    SceneEditorDocumentTransform adopted = before;
    adopted.position[0] += .25;
    lifecycle_t0_transform(&adopted);
    assert(RuntimeSurfaceGraphActive(0) && SceneEditorDocumentRevision() == revision + 1);
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    lifecycle_t0_transform(&before);
    assert(SceneEditorDocumentSave(diagnostic, sizeof(diagnostic)));

    FILE *receipt = fopen("surface_lifecycle_t0.json", "w");
    assert(receipt);
    fprintf(receipt, "{\"early_failures_preserved\":2,\"late_failures_cleared\":4,"
                     "\"snapshot_preserved\":true,\"history_rollback\":true,"
                     "\"restore_failure_explicit_empty\":true,\"retained_document_preserved\":true,"
                     "\"candidate_history_preserved\":true,\"candidate_prepare_rollback_and_adopt_undo\":true,\"empty_path_preserved\":true,\"recovery_verified\":true}\n");
    assert(fclose(receipt) == 0);
}
