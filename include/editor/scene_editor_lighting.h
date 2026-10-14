#ifndef SCENE_EDITOR_LIGHTING_H
#define SCENE_EDITOR_LIGHTING_H
#include <stdbool.h>
#include <stddef.h>
#include <json-c/json.h>
/* Shared retained UI/MCP commands. World-space position/radius are metres. */
bool SceneEditorLightingPrimaryEditable(char* message,size_t size);
json_object* SceneEditorLightingRead(void);
bool SceneEditorLightingGetPrimary(double position[3],double* intensity,double* radius);
bool SceneEditorLightingSetPrimary(const double position[3],double intensity,double radius,
    unsigned long long revision,char* message,size_t size);
bool SceneEditorLightingSetPrimaryValue(const char* property,double value);
bool SceneEditorLightingSetEnvironment(json_object* patch,unsigned long long revision,char* message,size_t size);
bool SceneEditorLightingEnvironmentValue(const char* property,double value);
bool SceneEditorLightingEnvironmentColor(double red,double green,double blue);
#endif
