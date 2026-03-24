#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "core_scene.h"

typedef struct SceneBundleImportResult {
    char fluid_source_path[4096];
    CoreSceneSourceType fluid_source_type;
    bool has_camera_path;
    char camera_path[4096];
    bool has_light_path;
    char light_path[4096];
    bool has_asset_mapping_profile;
    char asset_mapping_profile[128];
} SceneBundleImportResult;

bool scene_bundle_import_resolve_fluid_source(const char *bundle_path, SceneBundleImportResult *out_result);
