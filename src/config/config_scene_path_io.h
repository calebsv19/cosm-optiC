#ifndef CONFIG_SCENE_PATH_IO_H
#define CONFIG_SCENE_PATH_IO_H

#include <stdbool.h>
#include <json-c/json.h>

#include "config/config_manager.h"

struct json_object* config_scene_path_to_json_object(const Path* path);
void config_scene_save_path_to_json(struct json_object* config, const char* key, const Path* path);
bool config_scene_load_path_from_json_object(struct json_object* path_data, Path* out, bool allow_empty);
bool config_scene_load_path_from_json(struct json_object* config, const char* key, Path* out);
bool config_scene_load_camera_path_from_json(struct json_object* config, const char* key, Path* out);
void config_scene_save_path_depth_to_json(struct json_object* config,
                                          const char* key,
                                          const CameraPath3D* path3d,
                                          const Path* path);
bool config_scene_load_path_depth_from_json(struct json_object* config,
                                            const char* key,
                                            CameraPath3D* out_path3d,
                                            const Path* path);
void config_scene_save_camera_path_depth_to_json(struct json_object* config,
                                                 const char* key,
                                                 const CameraPath3D* path3d,
                                                 const Path* path);
bool config_scene_load_camera_path_depth_from_json(struct json_object* config,
                                                   const char* key,
                                                   CameraPath3D* out_path3d,
                                                   const Path* path);
void config_scene_ensure_camera_path_default(SceneConfig* scene);

#endif
