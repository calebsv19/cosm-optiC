#ifndef SCENE_EDITOR_ATMOSPHERE_GEOMETRY_H
#define SCENE_EDITOR_ATMOSPHERE_GEOMETRY_H
#include "editor/scene_editor_atmosphere.h"
#include "render/runtime_volume_3d.h"

/* Cache geometry stays in metres. Retained translations are in authored units.
 * UI transforms use the centre, converting back to the compatible origin pivot. */
bool SceneEditorAtmosphereGeometry(RuntimeVolumeAttachment3D* out,char* diagnostic,size_t size);
bool SceneEditorAtmosphereLoadGeometry(const SceneAtmosphereAttachment* value,
    RuntimeVolumeAttachment3D* out,char* diagnostic,size_t size);
void SceneEditorAtmospherePlaceGeometry(RuntimeVolumeAttachment3D* volume,
    const SceneAtmosphereAttachment* value,double world_scale);
Vec3 SceneEditorAtmosphereCentre(const RuntimeVolumeAttachment3D* volume);
bool SceneEditorAtmosphereSetCentre(SceneAtmosphereAttachment* value,
    const RuntimeVolumeAttachment3D* volume,Vec3 centre,double world_scale);
bool SceneEditorAtmosphereFit(SceneAtmosphereAttachment* value,
    const RuntimeVolumeAttachment3D* volume,const char* preset);
void SceneEditorAtmosphereGeometryReset(void);
#endif
