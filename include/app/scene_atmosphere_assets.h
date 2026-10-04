#ifndef SCENE_ATMOSPHERE_ASSETS_H
#define SCENE_ATMOSPHERE_ASSETS_H
#include "app/scene_atmosphere_attachment.h"
/* Every dependency is relative to its owning manifest and contained below the
 * scene directory. No symlink component is admitted. */
bool SceneAtmosphereAssetPath(const char* owner,const char* relative,const char* root,
    char* out,size_t size,char* diagnostic,size_t diagnostic_size);
bool SceneAtmosphereAssetsValidate(const SceneAtmosphereAttachment* value,
    const char* scene_path,char* diagnostic,size_t size);
bool SceneAtmosphereAssetVolumeSource(const char* scene,const char* source,char* path,size_t size,
    int* kind,char* diagnostic,size_t diagnostic_size);
#endif
