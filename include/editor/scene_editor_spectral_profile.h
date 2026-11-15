#ifndef EDITOR_SCENE_EDITOR_SPECTRAL_PROFILE_H
#define EDITOR_SCENE_EDITOR_SPECTRAL_PROFILE_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
/* Ordinary V3 profile authoring uses the same document transaction as native UI edits. */
json_object *SceneEditorSpectralProfileRead(void);
bool SceneEditorSpectralProfileSet(json_object *,unsigned long long,char *,size_t);
#endif
