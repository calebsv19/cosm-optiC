#ifndef SCENE_ATMOSPHERE_RUNTIME_H
#define SCENE_ATMOSPHERE_RUNTIME_H
#include "app/scene_atmosphere_attachment.h"
#include "render/runtime_volume_3d.h"
#include "animation/evaluated_scene_snapshot.h"
json_object* SceneAtmosphereAuthoring(json_object* scene);
bool SceneAtmosphereValidateScene(json_object* scene,char* diagnostic,size_t size);
void SceneAtmosphereRuntimeReset(void);
bool SceneAtmosphereRuntimeLoad(json_object* scene,double world_scale);
const SceneAtmosphereAttachment* SceneAtmosphereRuntimeRead(void);
void SceneAtmosphereRuntimeApplyTuning(void);
bool SceneAtmosphereRuntimeSource(const char* scene_path,char* path,size_t size,char* diagnostic,size_t diagnostic_size);
bool SceneAtmosphereRuntimeFrame(const char* scene_path,const TimelineEvaluationContext* frame,
    int64_t* selected,char* diagnostic,size_t size);
double SceneAtmosphereRuntimeWorldScale(void);
bool SceneAtmosphereRuntimeContext(int frame_index,TimelineEvaluationContext* context,char* diagnostic,size_t size);
/* Retained bundle sidecars use the same clock, but their own actual frame IDs. */
int SceneAtmosphereRuntimeWater(const char* scene_path,const TimelineEvaluationContext* frame,
    char* path,size_t size,int* frame_index,char* diagnostic,size_t diagnostic_size);
bool SceneAtmosphereRuntimeWaterDigest(const char* scene_path,const TimelineEvaluationContext* frame,
    char content[65],char manifest[65],char* diagnostic,size_t diagnostic_size);
/* 0 legacy, 1 handled (including disabled), -1 rejected. */
int SceneAtmosphereRuntimeAttach(RuntimeVolumeAttachment3D* volume,const char* scene_path,
    int frame_index,const TimelineEvaluationContext* frame,const RayEvaluatedSimulationIdentity* identity,
    char* diagnostic,size_t size);
bool SceneAtmosphereRuntimeSimulation(const char* scene_path,const TimelineEvaluationContext* frame,
    RayEvaluatedSimulationIdentity* identity,char* diagnostic,size_t size);
bool SceneAtmosphereRuntimeSnapshotMatches(const char* scene_path,const RayEvaluatedSceneSnapshot* snapshot,
    char* diagnostic,size_t size);
#endif
