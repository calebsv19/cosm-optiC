#ifndef SCENE_ATMOSPHERE_APPEARANCE_H
#define SCENE_ATMOSPHERE_APPEARANCE_H
#include "app/scene_atmosphere_attachment.h"

typedef enum {
    SCENE_ATMOSPHERE_LIGHT_CLOUD,
    SCENE_ATMOSPHERE_SOFT_MIST,
    SCENE_ATMOSPHERE_SMOKE,
    SCENE_ATMOSPHERE_LOOK_COUNT
} SceneAtmosphereLook;
const char* SceneAtmosphereLookName(SceneAtmosphereLook look);
/* Appearance only: source, placement, playback and visibility are preserved. */
bool SceneAtmosphereApplyLook(SceneAtmosphereAttachment* value, SceneAtmosphereLook look);
SceneAtmosphereLook SceneAtmosphereCurrentLook(const SceneAtmosphereAttachment* value);
void SceneAtmosphereTintHSV(const SceneAtmosphereAttachment* value, double hsv[3]);
bool SceneAtmosphereSetTintHSV(SceneAtmosphereAttachment* value, const double hsv[3]);
#endif
