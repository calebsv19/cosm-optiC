#ifndef CONFIG_SCENE_MATERIAL_PERSISTENCE_H
#define CONFIG_SCENE_MATERIAL_PERSISTENCE_H

#include <stdbool.h>
#include <json-c/json.h>

struct json_object* ConfigSaveMaterialTextureStackForObject(int scene_object_index);
struct json_object* SaveMaterialFacePlacementsForObject(int scene_object_index);
bool ConfigLoadMaterialTextureStack(struct json_object* obj, int scene_object_index);
void LoadMaterialFacePlacements(struct json_object* obj, int scene_object_index);

#endif
