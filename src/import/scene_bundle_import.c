#include "import/scene_bundle_import.h"

#include <stdio.h>
#include <string.h>

bool scene_bundle_import_resolve_fluid_source(const char *bundle_path, SceneBundleImportResult *out_result) {
    if (!bundle_path || !out_result) return false;

    CoreSceneBundleInfo info;
    CoreResult r = core_scene_bundle_resolve(bundle_path, &info);
    if (r.code != CORE_OK) return false;

    memset(out_result, 0, sizeof(*out_result));
    snprintf(out_result->fluid_source_path,
             sizeof(out_result->fluid_source_path),
             "%s",
             info.fluid_source_path);
    out_result->fluid_source_type = info.fluid_source_type;
    out_result->has_camera_path = info.has_camera_path;
    if (info.has_camera_path) {
        snprintf(out_result->camera_path, sizeof(out_result->camera_path), "%s", info.camera_path);
    }
    out_result->has_light_path = info.has_light_path;
    if (info.has_light_path) {
        snprintf(out_result->light_path, sizeof(out_result->light_path), "%s", info.light_path);
    }
    out_result->has_asset_mapping_profile = info.has_asset_mapping_profile;
    if (info.has_asset_mapping_profile) {
        snprintf(out_result->asset_mapping_profile,
                 sizeof(out_result->asset_mapping_profile),
                 "%s",
                 info.asset_mapping_profile);
    }
    return true;
}
