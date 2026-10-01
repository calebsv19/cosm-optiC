#include "editor/scene_editor_object_transform_preview.h"
#include "editor/scene_editor_object_timeline.h"
#include "import/runtime_scene_object_timeline.h"
static void path_viewport_rotation(SceneEditor *editor,const char *scene) {
  char message[256];MotionPaths paths;
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);authoring_control(editor,"paths");
  authoring_control(editor,"path_follower/object/obj_sphere_medium");
  SceneEditorDocumentObjectInfo info;assert(SceneEditorDocumentObjectById("obj_sphere_medium",&info));
  ObjectEditorSetSelectedObjectIndex(info.runtime_index);
  double first[3]={0};
  for(int axis=0;axis<6;++axis) {
    assert(SceneEditorMotionPathsRead(&paths));
    MotionPathBinding *binding=NULL;for(size_t j=0;j<paths.binding_count;++j)if(!strcmp(paths.bindings[j].object_id,info.id))binding=&paths.bindings[j];
    assert(binding && binding->follow_direction && binding->forward_axis==axis);
    const int frames[]={0,53,119,53};
    for(int k=0;k<4;++k) {
      SceneEditorTimelineSeek(frames[k]);SceneEditorSessionRuntimeRender(editor);
      const RayTracingRuntimeMeshAssetInstance *source=NULL;
      for(int j=0;j<SceneEditorMeshPreviewStoreInstanceCount();++j) {const RayTracingRuntimeMeshAssetInstance *m=SceneEditorMeshPreviewStoreGetInstance(j);if(m && !strcmp(m->object_id,info.id))source=m;}
      assert(source);RayTracingRuntimeMeshAssetInstance display;assert(SceneEditorObjectTransformPreviewMesh(source,&display));
      TimelineSample sample={0};TimelineRate rate={0};TimelineRange range={0};TimelineEvaluationContext context;
      assert(SceneEditorTimelineCurrentSample(&sample) && RuntimeSceneTimelineClock(&rate,&range)==TIMELINE_STATUS_OK);
      assert(TimelineEvaluationContextBuild(rate,range,sample,&context)==TIMELINE_STATUS_OK);
      RayEvaluatedObjectTransform transforms[64];size_t count=0;assert(RuntimeObjectTimelineCapture(&context,transforms,64,&count)==TIMELINE_STATUS_OK);
      RayEvaluatedObjectTransform *evaluated=NULL;for(size_t j=0;j<count;++j)if(!strcmp(transforms[j].target_id,info.id))evaluated=&transforms[j];
      assert(evaluated && evaluated->has_rotation);
      assert(fabs(display.rotation_x-evaluated->rotation_radians.x)<1e-12 && fabs(display.rotation_y-evaluated->rotation_radians.y)<1e-12 && fabs(display.rotation_z-evaluated->rotation_radians.z)<1e-12);
      if(axis==0 && k==0){first[0]=display.rotation_x;first[1]=display.rotation_y;first[2]=display.rotation_z;}
      if(axis==0 && k==1)assert(fabs(display.rotation_x-first[0])+fabs(display.rotation_y-first[1])+fabs(display.rotation_z-first[2])>0.01);
      assert(SceneEditorFrameViewport(true));SceneEditorSessionRuntimeRender(editor);
      SceneEditorDigestOverlayProjector projector=dm1_projector();int x,y;
      assert(SceneEditorDigestOverlayProjectPoint(&projector,display.position_x,display.position_y,display.position_z,&x,&y));
      assert(SceneEditorMeshPreviewPickObjectIndex(&projector,animSettings.editorMode,info.runtime_index,x,y)==info.runtime_index);
      RuntimeSceneBridge3DDigestState digest;assert(SceneEditorDigestOverlayResolve(&digest));double lo[3],hi[3],span;
      assert(SceneEditorDigestOverlayResolveObjectExtents(&digest,info.runtime_index,&lo[0],&lo[1],&lo[2],&hi[0],&hi[1],&hi[2],&span));
      const SceneEditorDigestOverlayNavState *nav=SceneEditorGetViewportNavState();
      assert(fabs(nav->target_x-(lo[0]+hi[0])/2)<1e-8 && fabs(nav->target_y-(lo[1]+hi[1])/2)<1e-8 && fabs(nav->target_z-(lo[2]+hi[2])/2)<1e-8);
      if(axis==0 && k<3){char name[80];snprintf(name,sizeof(name),"viewport_heading_%d.ppm",frames[k]);capture(editor,name);}
      if(k==0 && (axis==2 || axis==4)){char name[80];snprintf(name,sizeof(name),"viewport_axis_%d.ppm",axis);capture(editor,name);}
    }
    dm4_inspector_control(editor,"path_forward_axis");
  }
  dm4_inspector_control(editor,"path_follow_direction");
  {const RayTracingRuntimeMeshAssetInstance *source=NULL;for(int j=0;j<SceneEditorMeshPreviewStoreInstanceCount();++j){const RayTracingRuntimeMeshAssetInstance *m=SceneEditorMeshPreviewStoreGetInstance(j);if(m && !strcmp(m->object_id,info.id))source=m;}
   assert(source);RayTracingRuntimeMeshAssetInstance display;assert(SceneEditorObjectTransformPreviewMesh(source,&display));
   assert(display.rotation_x==source->rotation_x && display.rotation_y==source->rotation_y && display.rotation_z==source->rotation_z);}
  dm4_inspector_control(editor,"path_follow_direction");choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());
  assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));SceneEditorTimelineSeek(53);SceneEditorSessionRuntimeRender(editor);
  double rotation[3];assert(SceneEditorObjectTimelineRotation(info.id,rotation));
  fprintf(stderr,"Viewport heading PASS: six axes, timeline seek/reseek, evaluated-pose parity, pick, frame, off/base restoration and save/reopen.\n");
}
