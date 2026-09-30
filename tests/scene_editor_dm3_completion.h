#include "app/evaluated_camera_route.h"
static RayEvaluatedSceneSnapshot dm3_complete_sample(int frame, unsigned sub) {
  RayEvaluatedSceneServiceResult result;
  bool ok=RayEvaluatedSceneCaptureSample((TimelineSample){frame,sub,8},&result);
  if(!ok)fprintf(stderr,"M3 sample: %s\n",result.status_line);
  assert(ok);return result.snapshot;
}
static void dm3_focus_check(const RayEvaluatedCamera *c) {
  RuntimeSceneBridge3DScaffoldState scaffold;
  runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
  MotionPathBinding binding;
  if(!scaffold.has_camera_focus_target || !MotionPathsRuntimeBinding("camera/main",&binding) || !binding.use_focus_target)return;
  double dx=scaffold.camera_focus_target_x-c->position.x,dy=scaffold.camera_focus_target_y-c->position.y,dz=scaffold.camera_focus_target_z-c->position.z;
  double pitch=fmax(-70*M_PI/180,fmin(70*M_PI/180,atan2(dz,hypot(dx,dy))));
  assert(fabs(c->yaw_radians-atan2(dx,-dy))<1e-8 && fabs(c->pitch_radians-pitch)<1e-8);
}
static json_object *dm3_complete_row(RayEvaluatedSceneSnapshot *s,bool objects) {
  json_object *row=json_object_new_array();
  double v[]={s->camera.position.x,s->camera.position.y,s->camera.position.z,s->camera.yaw_radians,s->camera.pitch_radians,s->camera.fov_y_degrees,
    s->light.position.x,s->light.position.y,s->light.position.z,s->light.intensity};
  for(size_t i=0;i<10;++i)json_object_array_add(row,json_object_new_double(v[i]));
  if(objects) {
    bool found=false;
    for(size_t i=0;i<s->object_transform_count;++i)if(!strcmp(s->object_transforms[i].target_id,"obj_sphere_medium")) {
      TimelineVec3 p=s->object_transforms[i].position;json_object_array_add(row,json_object_new_double(p.x));json_object_array_add(row,json_object_new_double(p.y));json_object_array_add(row,json_object_new_double(p.z));found=true;
    }
    assert(found);
  }
  return row;
}
static void dm3_save_copy(const char *scene,const char *name) {
  char message[256];assert(SceneEditorDocumentSave(message,sizeof(message)));
  json_object *s=json_object_from_file(scene);assert(s);assert(json_object_to_file_ext(name,s,JSON_C_TO_STRING_PRETTY)==0);json_object_put(s);
}
static void dm3_completion(SceneEditor *editor,const char *scene,bool reopen) {
  const int frames[]={0,23,60,119};char message[256];MotionPaths paths;
  static TimelineDocument doc,prior;
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
  if(reopen) {
    json_object *expected=json_object_from_file("dm3_combined_expected.json");assert(expected);
    for(int i=3;i>=0;--i) {
      RayEvaluatedSceneSnapshot s=dm3_complete_sample(frames[i],0);dm3_focus_check(&s.camera);
      json_object *row=dm3_complete_row(&s,true),*reference=json_object_array_get_idx(expected,i);
      assert(json_object_array_length(row)==13);
      for(int k=0;k<13;++k)assert(fabs(json_object_get_double(json_object_array_get_idx(row,k))-json_object_get_double(json_object_array_get_idx(reference,k)))<1e-8);
      json_object_put(row);
    }
    json_object_put(expected);fprintf(stderr,"D-M3 combined reopen PASS\n");return;
  }
  assert(SceneEditorTimelineActivate());assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  for(size_t i=0;i<doc.track_count;++i) {
    TimelineTrack *t=&doc.tracks[i];
    if(!strcmp(t->property_id,"camera/path_progress")) {
      assert(TimelineTrackInsertKey(t,(TimelineKeyframe){.frame=30,.value=TimelineValueScalar(.25),.interpolation_to_next=TIMELINE_INTERPOLATION_STEP},&(size_t){0})==TIMELINE_STATUS_OK);
      assert(TimelineTrackInsertKey(t,(TimelineKeyframe){.frame=60,.value=TimelineValueScalar(.55),.interpolation_to_next=TIMELINE_INTERPOLATION_LINEAR},&(size_t){0})==TIMELINE_STATUS_OK);
    }
    if(!strcmp(t->property_id,"camera/fov_y"))assert(TimelineTrackInsertKey(t,(TimelineKeyframe){.frame=60,.value=TimelineValueScalar(47),.interpolation_to_next=TIMELINE_INTERPOLATION_LINEAR},&(size_t){0})==TIMELINE_STATUS_OK);
  }
  TimelineTrack intensity;assert(TimelineTrackInit(&intensity,"combined-intensity","light/light_key","light/intensity",TIMELINE_VALUE_SCALAR)==TIMELINE_STATUS_OK);
  TimelineTrackSetUnit(&intensity,TIMELINE_UNIT_RELATIVE_INTENSITY);
  TimelineTrackAddKey(&intensity,0,TimelineValueScalar(2.8),TIMELINE_INTERPOLATION_LINEAR);TimelineTrackAddKey(&intensity,119,TimelineValueScalar(4.2),TIMELINE_INTERPOLATION_LINEAR);
  assert(TimelineDocumentAddTrack(&doc,&intensity)==TIMELINE_STATUS_OK);
  assert(SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),message,sizeof(message)));
  assert(SceneEditorDocumentGetTimeline(&prior)==TIMELINE_STATUS_OK);
  dm3_save_copy(scene,"dm3_conversion_before.scene.json");
  static RayEvaluatedCamera before_camera[953];static RayEvaluatedLight before_light[953];
  for(int i=0;i<953;++i){RayEvaluatedSceneSnapshot s=dm3_complete_sample(i/8,i%8);before_camera[i]=s.camera;before_light[i]=s.light;dm3_focus_check(&s.camera);}
  unsigned long long rev=SceneEditorDocumentRevision();
  assert(!SceneEditorMotionPathConvertLegacy(true,rev-1,message,sizeof(message)) && SceneEditorDocumentRevision()==rev);
  doc=prior;doc.range.frame_count=4097;
  assert(SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),message,sizeof(message)));
  unsigned long long refused=SceneEditorDocumentRevision();
  assert(!SceneEditorMotionPathConvertLegacy(true,refused,message,sizeof(message)) && SceneEditorDocumentRevision()==refused);
  choose_menu(editor,1,0);
  doc=prior;
  for(size_t i=0;i<doc.track_count;++i)if(!strcmp(doc.tracks[i].property_id,"camera/path_progress")) {
    TimelineTrack *t=&doc.tracks[i];t->key_count=2;
    t->keys[0]=(TimelineKeyframe){.frame=0,.value=TimelineValueScalar(0),.interpolation_to_next=TIMELINE_INTERPOLATION_CUBIC_BEZIER,.outgoing_frame_offset=.33};
    t->keys[1]=(TimelineKeyframe){.frame=1,.value=TimelineValueScalar(1),.interpolation_to_next=TIMELINE_INTERPOLATION_STEP,.incoming_frame_offset=-.33};
  }
  assert(SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),message,sizeof(message)));
  refused=SceneEditorDocumentRevision();
  assert(!SceneEditorMotionPathConvertLegacy(true,refused,message,sizeof(message)) && SceneEditorDocumentRevision()==refused);
  assert(strstr(message,"tolerance"));
  choose_menu(editor,1,0);rev=SceneEditorDocumentRevision();
  authoring_control(editor,"paths");authoring_control(editor,"path_followers");authoring_control(editor,"path_follower_camera");authoring_control(editor,"path_camera_convert");
  assert(SceneEditorDocumentRevision()==rev+1);
  fprintf(stderr,"M3 camera conversion committed\n");
  assert(SceneEditorMotionPathsRead(&paths) && paths.count==1 && paths.binding_count==1);
  choose_menu(editor,1,0);assert(SceneEditorMotionPathsRead(&paths) && !paths.count);
  choose_menu(editor,1,1);assert(SceneEditorMotionPathsRead(&paths) && paths.count==1);
  authoring_control(editor,"path_followers");
  authoring_control(editor,"path_follower_light");rev=SceneEditorDocumentRevision();authoring_control(editor,"path_light_convert");assert(SceneEditorDocumentRevision()==rev+1);
  assert(SceneEditorMotionPathsRead(&paths) && paths.count==2 && paths.binding_count==2);
  double maximum=0;
  for(int i=0;i<953;++i) {
    RayEvaluatedSceneSnapshot s=dm3_complete_sample(i/8,i%8);
    double a[]={s.camera.position.x-before_camera[i].position.x,s.camera.position.y-before_camera[i].position.y,s.camera.position.z-before_camera[i].position.z,
      s.light.position.x-before_light[i].position.x,s.light.position.y-before_light[i].position.y,s.light.position.z-before_light[i].position.z};
    for(int k=0;k<6;++k)maximum=fmax(maximum,fabs(a[k]));
    assert(maximum<.003*SceneEditorDocumentWorldScale());
    assert(fabs(s.camera.fov_y_degrees-before_camera[i].fov_y_degrees)<1e-8);
    assert(fabs(s.camera.yaw_radians-before_camera[i].yaw_radians)<1e-8);
    assert(fabs(s.camera.pitch_radians-before_camera[i].pitch_radians)<1e-8);
    assert(fabs(s.light.intensity-before_light[i].intensity)<1e-8);dm3_focus_check(&s.camera);
  }
  fprintf(stderr,"M3 conversion sampled maximum position error %.12g world units\n",maximum);
  assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  for(size_t i=0;i<prior.track_count;++i) {
    TimelineTrack expected=prior.tracks[i];
    if(!strcmp(expected.property_id,"camera/path_progress") || !strcmp(expected.property_id,"light/path_progress"))expected.enabled=false;
    assert(!memcmp(&expected,&doc.tracks[i],sizeof(expected)));
  }
  dm3_save_copy(scene,"dm3_converted.scene.json");
  rev=SceneEditorDocumentRevision();assert(!SceneEditorMotionPathConvertLegacy(true,rev,message,sizeof(message)) && SceneEditorDocumentRevision()==rev);
  // All three follower kinds share one route, each with independent progress.
  char common[64];snprintf(common,sizeof(common),"%s",paths.paths[0].id);
  assert(SceneEditorMotionPathBindLight(common,true,SceneEditorDocumentRevision(),message,sizeof(message)));
  assert(SceneEditorMotionPathBind("obj_sphere_medium",common,true,SceneEditorDocumentRevision(),message,sizeof(message)));
  assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  for(size_t i=0;i<doc.track_count;++i)if(!strcmp(doc.tracks[i].property_id,MOTION_PROGRESS_PROPERTY)) {
    doc.tracks[i].keys[0].value=TimelineValueScalar(.8);doc.tracks[i].keys[1].value=TimelineValueScalar(.2);
  }
  assert(SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),message,sizeof(message)));
  assert(SceneEditorMotionPathsRead(&paths));
  paths.paths[0].points[1].position[2]+=.4;
  assert(SceneEditorMotionPathsSet(&paths,SceneEditorDocumentRevision(),message,sizeof(message)));
  static TimelineDocument unchanged;assert(SceneEditorDocumentGetTimeline(&unchanged)==TIMELINE_STATUS_OK);
  assert(!memcmp(&doc,&unchanged,sizeof(doc)));
  for(size_t i=0;i<doc.track_count;++i)if(!strcmp(doc.tracks[i].property_id,MOTION_CAMERA_PROGRESS_PROPERTY))SceneEditorTimelineSelectTrack(i);
  SceneEditorMotionPathPanelSelect(true);SceneEditorSessionRuntimeRender(editor);
  authoring_control(editor,"path_followers");authoring_control(editor,"path_follower_camera");authoring_control(editor,"path_camera_focus");
  MotionPathBinding focus_binding;assert(MotionPathsRuntimeBinding("camera/main",&focus_binding) && focus_binding.use_focus_target);
  authoring_control(editor,"path_camera_focus");assert(MotionPathsRuntimeBinding("camera/main",&focus_binding) && !focus_binding.use_focus_target);
  authoring_control(editor,"path_camera_focus");
  RuntimeSceneBridge3DScaffoldState scaffold;runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
  PreviewCameraSample focus_edge={.position_x=scaffold.camera_focus_target_x,.position_y=scaffold.camera_focus_target_y,.position_z=scaffold.camera_focus_target_z,.yaw_radians=.4,.pitch_radians=.2};
  EvaluatedCameraApplyFocusTarget(&focus_edge);assert(focus_edge.yaw_radians==.4 && focus_edge.pitch_radians==.2);
  focus_edge.position_z-=10;EvaluatedCameraApplyFocusTarget(&focus_edge);assert(fabs(focus_edge.pitch_radians-70*M_PI/180)<1e-8);
  json_object *expected=json_object_new_array();
  for(int i=0;i<4;++i) {
    RayEvaluatedSceneSnapshot s=dm3_complete_sample(frames[i],0);dm3_focus_check(&s.camera);
    json_object_array_add(expected,dm3_complete_row(&s,true));
  }
  assert(json_object_to_file_ext("dm3_combined_expected.json",expected,JSON_C_TO_STRING_PRETTY)==0);json_object_put(expected);
  dm3_save_copy(scene,"dm3_combined.scene.json");
  // Detach restores each target independently; undo returns the whole binding.
  assert(SceneEditorMotionPathBindLight(common,false,SceneEditorDocumentRevision(),message,sizeof(message)));
  for(int i=0;i<4;++i){RayEvaluatedSceneSnapshot s=dm3_complete_sample(frames[i],0);assert(fabs(s.light.position.x-before_light[frames[i]*8].position.x)<1e-8);}
  choose_menu(editor,1,0);
  assert(SceneEditorMotionPathBindCamera(common,false,SceneEditorDocumentRevision(),message,sizeof(message)));
  for(int i=0;i<4;++i){RayEvaluatedSceneSnapshot s=dm3_complete_sample(frames[i],0);assert(fabs(s.camera.position.z-before_camera[frames[i]*8].position.z)<1e-8);}
  choose_menu(editor,1,0);assert(SceneEditorDocumentSave(message,sizeof(message)));
  assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  for(size_t i=0;i<doc.track_count;++i)if(!strcmp(doc.tracks[i].property_id,MOTION_CAMERA_PROGRESS_PROPERTY))SceneEditorTimelineSelectTrack(i);
  SceneEditorMotionPathPanelSelect(true);SceneEditorSessionRuntimeRender(editor);
  SDL_Rect fit;assert(SceneEditorTimelineControl("fit",&fit));click(editor,fit);
  SDL_SetWindowSize(editor->window,1024,640);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);SceneEditorSessionRuntimeRender(editor);
  authoring_control(editor,"path_camera_focus");authoring_control(editor,"path_camera_focus");
  assert(MotionPathsRuntimeBinding("camera/main",&focus_binding) && focus_binding.use_focus_target);
  capture(editor,"dm3_compact.ppm");
  SDL_SetWindowSize(editor->window,1280,800);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);SceneEditorSessionRuntimeRender(editor);
  capture(editor,"dm3_combined.ppm");
  assert(SceneEditorDocumentSave(message,sizeof(message)));
  fprintf(stderr,"D-M3 completion PASS: atomic conversion, sampled equivalence, source identity, combined mesh/camera/light, focus composition, independent timing, shape edit and detach/undo\n");
}
