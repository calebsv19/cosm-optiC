#include "ui/volume_source_ui_labels.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "config/config_manager.h"
#include "app/scene_atmosphere_runtime.h"
#include "import/runtime_scene_volume_defaults.h"
#include "ui/sdl_menu_state.h"

bool volume_source_ui_read_scene_attachment(SceneAtmosphereAttachment* out) {
    static char cached_path[PATH_MAX];
    static struct stat cached_stat;
    static SceneAtmosphereAttachment cached;
    static bool cached_valid;
    struct stat current;
    if (!out) return false;
    SceneAtmosphereAttachmentDefaults(out);
    if (animSettings.sceneSource != SCENE_SOURCE_RUNTIME_SCENE) return false;
    if (!animSettings.runtimeScenePath[0]) {
        *out = *SceneAtmosphereRuntimeRead();
        return out->present;
    }
    if (stat(animSettings.runtimeScenePath, &current) != 0) return false;
#ifdef __APPLE__
#define MODIFIED(s) (s).st_mtimespec
#else
#define MODIFIED(s) (s).st_mtim
#endif
    if (strcmp(cached_path, animSettings.runtimeScenePath) ||
        cached_stat.st_ino != current.st_ino || cached_stat.st_size != current.st_size ||
        MODIFIED(cached_stat).tv_sec != MODIFIED(current).tv_sec ||
        MODIFIED(cached_stat).tv_nsec != MODIFIED(current).tv_nsec) {
        json_object* scene = json_object_from_file(animSettings.runtimeScenePath);
        SceneAtmosphereAttachmentDefaults(&cached);
        cached_valid = scene && SceneAtmosphereAttachmentParse(
            SceneAtmosphereAuthoring(scene), &cached, NULL, 0);
        if (scene) json_object_put(scene);
        snprintf(cached_path, sizeof(cached_path), "%s", animSettings.runtimeScenePath);
        cached_stat = current;
    }
#undef MODIFIED
    if (cached_valid) *out = cached;
    return cached_valid && out->present;
}

static const char* volume_kind_label(int kind) {
    switch (animation_config_volume_source_kind_clamp(kind)) {
        case VOLUME_SOURCE_MANIFEST:
            return "Manifest";
        case VOLUME_SOURCE_RAW_VF3D:
            return "VF3D";
        case VOLUME_SOURCE_PACK:
            return "Pack";
        case VOLUME_SOURCE_NONE:
        default:
            return "None";
    }
}

static bool volume_source_is_auto_paired(void) {
    int default_kind = VOLUME_SOURCE_NONE;
    char default_path[PATH_MAX] = {0};
    const int current_kind = animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind);

    if (animation_config_scene_source_clamp(animSettings.sceneSource) != SCENE_SOURCE_RUNTIME_SCENE) {
        return false;
    }
    if (!animSettings.runtimeScenePath[0] || !animSettings.volumeSourcePath[0]) {
        return false;
    }
    if (!runtime_scene_volume_defaults_resolve(animSettings.runtimeScenePath,
                                               &default_kind,
                                               default_path,
                                               sizeof(default_path))) {
        return false;
    }
    return current_kind == animation_config_volume_source_kind_clamp(default_kind) &&
           strcmp(animSettings.volumeSourcePath, default_path) == 0;
}

void volume_source_ui_format_catalog_option_label(const char* path,
                                                  int kind,
                                                  char* out,
                                                  size_t out_size) {
    char label[128] = {0};
    if (!out || out_size == 0u) return;
    out[0] = '\0';
    menu_state_build_manifest_label(path, label, sizeof(label));
    snprintf(out, out_size, "%s: %s", volume_kind_label(kind), label);
}

void volume_source_ui_format_active_button_label(char* out, size_t out_size) {
    char label[128] = {0};
    const char *badge = "None";
    if (!out || out_size == 0u) return;
    out[0] = '\0';

    SceneAtmosphereAttachment saved;
    volume_source_ui_read_scene_attachment(&saved);
    const SceneAtmosphereAttachment* v=&saved;
    if(v->present) {
        menu_state_build_manifest_label(v->source_path,label,sizeof(label));
        snprintf(out,out_size,"Scene atmosphere [%s]: %s",v->enabled?"On":"Off",label);
        return;
    }
    if (!animSettings.volumeSourcePath[0] ||
        animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind) == VOLUME_SOURCE_NONE) {
        snprintf(out, out_size, "Attach Volume [None]");
        return;
    }

    menu_state_build_manifest_label(animSettings.volumeSourcePath, label, sizeof(label));
    if (volume_source_is_auto_paired()) {
        badge = animSettings.volumeInteractionEnabled ? "Auto" : "Auto Off";
    } else {
        badge = animSettings.volumeInteractionEnabled ? "Custom" : "Custom Off";
    }
    snprintf(out, out_size, "Attach Volume [%s]: %s", badge, label);
}

void volume_source_ui_format_attach_status(int kind,
                                           const char* path,
                                           bool enabled,
                                           char* out,
                                           size_t out_size) {
    char label[128] = {0};
    if (!out || out_size == 0u) return;
    out[0] = '\0';

    if (!path || !path[0]) {
        snprintf(out, out_size, "Volume cleared");
        return;
    }

    menu_state_build_manifest_label(path, label, sizeof(label));
    snprintf(out,
             out_size,
             "%s volume %s: %s",
             volume_kind_label(kind),
             enabled ? "attached" : "stored",
             label);
}
