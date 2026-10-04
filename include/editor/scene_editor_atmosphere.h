#ifndef SCENE_EDITOR_ATMOSPHERE_H
#define SCENE_EDITOR_ATMOSPHERE_H
#include "app/scene_atmosphere_attachment.h"
bool SceneEditorAtmosphereRead(SceneAtmosphereAttachment* out);
bool SceneEditorAtmosphereSet(const SceneAtmosphereAttachment* value,unsigned long long revision,char* diagnostic,size_t size);
bool SceneEditorAtmosphereRemove(unsigned long long revision,char* diagnostic,size_t size);
bool SceneEditorAtmosphereImport(const char* source,bool preset,unsigned long long revision,char* diagnostic,size_t size);
#endif
