#ifndef CONFIG_SCENE_PATH_IO_H
#define CONFIG_SCENE_PATH_IO_H

#include <stdbool.h>
#include <json-c/json.h>

#include "config/config_manager.h"

void config_scene_save_path_to_json(struct json_object* config, const char* key, const Path* path);
bool config_scene_load_path_from_json(struct json_object* config, const char* key, Path* out);
void config_scene_ensure_camera_path_default(SceneConfig* scene);

#endif
