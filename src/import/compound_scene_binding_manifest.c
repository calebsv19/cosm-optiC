#include "import/compound_scene_binding_manifest.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool finite_vec3(RayCompoundSceneVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool identity_orientation(RayCompoundSceneQuat value) {
    return isfinite(value.w) && isfinite(value.x) && isfinite(value.y) &&
        isfinite(value.z) && fabs(value.w - 1.0) <= 1e-12 &&
        fabs(value.x) <= 1e-12 && fabs(value.y) <= 1e-12 &&
        fabs(value.z) <= 1e-12;
}

void ray_compound_scene_binding_manifest_init(
    RayCompoundSceneBindingManifest* manifest,
    const RayCompoundSceneHandoff* handoff) {
    if (!manifest)
        return;
    memset(manifest, 0, sizeof(*manifest));
    snprintf(manifest->schema, sizeof(manifest->schema), "%s",
        RAY_COMPOUND_SCENE_BINDING_MANIFEST_SCHEMA);
    snprintf(manifest->composition, sizeof(manifest->composition), "%s",
        RAY_COMPOUND_SCENE_BINDING_COMPOSITION);
    snprintf(manifest->playback, sizeof(manifest->playback), "%s",
        RAY_COMPOUND_SCENE_BINDING_PLAYBACK);
    manifest->world_from_simulation_orientation.w = 1.0;
    if (!handoff || !ray_compound_scene_handoff_validate(handoff))
        return;
    manifest->handoff_digest = handoff->handoff_digest;
    manifest->binding_count = RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT;
    for (size_t i = 0; i < manifest->binding_count; ++i) {
        RayCompoundSceneRendererBinding* target = &manifest->bindings[i];
        const RayCompoundSceneSourceBinding* source = &handoff->bindings[i];
        target->body_id = source->body_id;
        snprintf(target->source_asset_id, sizeof(target->source_asset_id),
            "%s", source->source_asset_id);
        snprintf(target->source_sha256, sizeof(target->source_sha256),
            "%s", source->source_sha256);
    }
}

bool ray_compound_scene_binding_manifest_validate(
    const RayCompoundSceneBindingManifest* manifest,
    const RayCompoundSceneHandoff* handoff) {
    if (!manifest || !ray_compound_scene_handoff_validate(handoff) ||
        strcmp(manifest->schema,
            RAY_COMPOUND_SCENE_BINDING_MANIFEST_SCHEMA) ||
        manifest->handoff_digest != handoff->handoff_digest ||
        strcmp(manifest->composition,
            RAY_COMPOUND_SCENE_BINDING_COMPOSITION) ||
        strcmp(manifest->playback, RAY_COMPOUND_SCENE_BINDING_PLAYBACK) ||
        !finite_vec3(manifest->world_from_simulation_translation_m) ||
        fabs(manifest->world_from_simulation_translation_m.x) > 1e-12 ||
        fabs(manifest->world_from_simulation_translation_m.y) > 1e-12 ||
        fabs(manifest->world_from_simulation_translation_m.z) > 1e-12 ||
        !identity_orientation(
            manifest->world_from_simulation_orientation) ||
        manifest->binding_count != RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT)
        return false;

    for (size_t i = 0; i < manifest->binding_count; ++i) {
        const RayCompoundSceneRendererBinding* binding =
            &manifest->bindings[i];
        const RayCompoundSceneSourceBinding* source = &handoff->bindings[i];
        if (binding->body_id != source->body_id ||
            strcmp(binding->source_asset_id, source->source_asset_id) ||
            strcmp(binding->source_sha256, source->source_sha256) ||
            !binding->object_id[0] || !binding->mesh_asset_id[0])
            return false;
        for (size_t prior = 0; prior < i; ++prior) {
            if (binding->body_id == manifest->bindings[prior].body_id ||
                !strcmp(binding->object_id,
                    manifest->bindings[prior].object_id))
                return false;
        }
    }
    return true;
}

const RayCompoundSceneRendererBinding*
ray_compound_scene_binding_manifest_find_body(
    const RayCompoundSceneBindingManifest* manifest,
    int body_id) {
    if (!manifest)
        return NULL;
    for (size_t i = 0; i < manifest->binding_count &&
         i < RAY_COMPOUND_SCENE_HANDOFF_BODY_COUNT; ++i) {
        if (manifest->bindings[i].body_id == body_id)
            return &manifest->bindings[i];
    }
    return NULL;
}
