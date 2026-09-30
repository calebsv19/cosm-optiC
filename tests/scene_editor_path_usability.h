#include "editor/scene_editor_motion_path_panel_internal.h"
#include "import/runtime_scene_object_timeline.h"
#include "editor/scene_editor_motion_plan_panel.h"
/* Human-facing path identity and compact-library acceptance. */
static void path_usability_library(SceneEditor *editor, const char *scene) {
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
  authoring_control(editor,"paths");
  MotionPaths paths;
  assert(SceneEditorMotionPathsRead(&paths) && paths.count == 0);
  authoring_control(editor,"new_path");
  assert(SceneEditorMotionPathsRead(&paths) && paths.count == 1);
  assert(!strcmp(paths.paths[0].name,"Path 1"));
  key(editor,SDLK_ESCAPE);
  authoring_control(editor,"new_path");
  assert(SceneEditorMotionPathsRead(&paths) && paths.count == 2);
  assert(!strcmp(paths.paths[0].name,"Path 1") && !strcmp(paths.paths[1].name,"Path 2"));
  authoring_text(editor,"Camera orbit");
  assert(SceneEditorMotionPathsRead(&paths) && !strcmp(paths.paths[1].name,"Camera orbit"));
  dm2_repair_select_row(editor,paths.paths[0].id);
  authoring_control(editor,"path_name");
  authoring_text(editor,"Object approach");
  assert(SceneEditorMotionPathsRead(&paths));
  assert(!strcmp(paths.paths[0].name,"Object approach"));
  assert(!strcmp(paths.paths[1].name,"Camera orbit"));
  choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());
  {json_object *saved=json_object_from_file(scene);MotionPaths retained;char error[256];
   assert(saved && MotionPathsParse(MotionPlansAuthor(saved),&retained,error,sizeof(error)) && retained.count==2 && !retained.binding_count);json_object_put(saved);}
  /* Continue the same scene through point creation, depth movement and smoothing. */
  key(editor,SDLK_ESCAPE);authoring_control(editor,"path_add");
  authoring_control(editor,"path_y");authoring_text(editor,"0.4");
  authoring_control(editor,"path_frame_selected");
  SDL_Rect gizmo;assert(SceneEditorRenderAuthoringControl("path_gizmo_z",&gizmo));
  int gx=gizmo.x+gizmo.w/2,gy=gizmo.y+gizmo.h/2;
  SDL_Event drag={.type=SDL_MOUSEBUTTONDOWN};drag.button.button=SDL_BUTTON_LEFT;drag.button.x=gx;drag.button.y=gy;
  unsigned long long before_drag=SceneEditorDocumentRevision();
  SceneEditorSessionRuntimeHandleEvent(editor,&drag);
  drag=(SDL_Event){.type=SDL_MOUSEMOTION};drag.motion.state=SDL_BUTTON_LMASK;drag.motion.x=gx;drag.motion.y=gy-18;
  SceneEditorSessionRuntimeHandleEvent(editor,&drag);
  drag=(SDL_Event){.type=SDL_MOUSEBUTTONUP};drag.button.button=SDL_BUTTON_LEFT;drag.button.x=gx;drag.button.y=gy-18;
  SceneEditorSessionRuntimeHandleEvent(editor,&drag);SceneEditorSessionRuntimeRender(editor);
  assert(SceneEditorDocumentRevision()==before_drag+1);
  authoring_control(editor,"path_corner");authoring_control(editor,"path_smooth");
  assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].count==3 && paths.paths[0].points[1].handle_mode==MOTION_HANDLE_LINKED);
  SDL_SetWindowSize(editor->window,1024,640);SDL_PumpEvents();
  SceneEditorSessionRuntimeRender(editor);SceneEditorSessionRuntimeRender(editor);
  const char *controls[]={"new_path","path_name","path_actions","path_select_tool","path_place_tool","path_frame_selected","path_followers","path_x","path_y","path_z","path_smooth"};
  SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));
  for(size_t i=0;i<sizeof(controls)/sizeof(controls[0]);++i) {
    SDL_Rect rect;assert(SceneEditorRenderAuthoringControl(controls[i],&rect));
    SDL_Rect pane=i<7?layout.left_content_rect:layout.right_content_rect;
    assert(rect.y>=pane.y && rect.y+rect.h<=pane.y+pane.h);
  }
  capture(editor,"path_library_compact.ppm");
  /* Hover uses the same point picker as clicks, without creating history. */
  { int hx,hy;assert(MotionPathPanelProject(paths.paths[0].points[0].position,&hx,&hy));
    unsigned long long revision=SceneEditorDocumentRevision();
    SDL_Event hover={.type=SDL_MOUSEMOTION};hover.motion.x=hx;hover.motion.y=hy;
    SceneEditorSessionRuntimeHandleEvent(editor,&hover);SceneEditorSessionRuntimeRender(editor);
    assert(motion_path_ui.hover_point==0 && SceneEditorDocumentRevision()==revision);
    capture(editor,"path_hover.ppm");
    hover.motion.x=0;hover.motion.y=0;SceneEditorSessionRuntimeHandleEvent(editor,&hover);
    assert(motion_path_ui.hover_point==-1);
  }
  /* Typed attachment inspectors expose only the selected target kind. */
  authoring_control(editor,"path_followers");
  dm4_inspector_control(editor,"path_follower_camera");
  SDL_Rect hidden;
  assert(!SceneEditorRenderAuthoringControl("path_attach",&hidden));
  assert(!SceneEditorRenderAuthoringControl("path_light_attach",&hidden));
  dm4_inspector_control(editor,"path_camera_attach");
  assert(SceneEditorMotionPathsRead(&paths) && paths.binding_count==1);
  dm4_inspector_control(editor,"path_follower_light");
  assert(!SceneEditorRenderAuthoringControl("path_camera_attach",&hidden));
  dm4_inspector_control(editor,"path_light_attach");
  assert(SceneEditorMotionPathsRead(&paths) && paths.binding_count==2);
  dm4_inspector_control(editor,"path_follower_object");
  dm4_inspector_control(editor,"path_object_picker");
  dm4_inspector_control(editor,"path_object/obj_sphere_medium");
  dm4_inspector_control(editor,"path_attach");
  assert(SceneEditorMotionPathsRead(&paths) && paths.binding_count==3);
  assert(!strcmp(paths.bindings[2].object_id,"obj_sphere_medium"));
  dm4_inspector_control(editor,"path_follow_direction");
  assert(SceneEditorMotionPathsRead(&paths) && paths.bindings[2].follow_direction);
  /* All six model axes align with the sampled route tangent. */
  for(int axis=0;axis<6;++axis) {
    TimelineVec3 rotation,a,b;
    assert(RuntimeObjectTimelineRotationAtT("obj_sphere_medium",0.37,&rotation));
    assert(MotionPathsRuntimeTargetPosition("object/obj_sphere_medium",0.37-1e-5,&a));
    assert(MotionPathsRuntimeTargetPosition("object/obj_sphere_medium",0.37+1e-5,&b));
    double v[3]={0};v[axis/2]=axis%2?-1:1;
    double x=v[0],y=v[1],z=v[2];
    double yy=y*cos(rotation.x)-z*sin(rotation.x),zz=y*sin(rotation.x)+z*cos(rotation.x);
    double xx=x*cos(rotation.y)+zz*sin(rotation.y);z=-x*sin(rotation.y)+zz*cos(rotation.y);
    x=xx*cos(rotation.z)-yy*sin(rotation.z);y=xx*sin(rotation.z)+yy*cos(rotation.z);
    double dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z,n=sqrt(dx*dx+dy*dy+dz*dz);
    assert((x*dx+y*dy+z*dz)/n>0.99999);
    dm4_inspector_control(editor,"path_forward_axis");
  }
  dm4_inspector_control(editor,"path_rotation_from_base");
  dm4_inspector_control(editor,"path_rotation_z");authoring_text(editor,"30");
  assert(SceneEditorMotionPathsRead(&paths) && paths.bindings[2].rotation_offset[2]==30);
  {TimelineVec3 a,b,junk;assert(RuntimeObjectTimelineRotationAtT("obj_sphere_medium",0.2,&a));
   assert(RuntimeObjectTimelineRotationAtT("obj_sphere_medium",0.9,&junk));
   assert(RuntimeObjectTimelineRotationAtT("obj_sphere_medium",0.2,&b));assert(!memcmp(&a,&b,sizeof(a)));}
  const char *followers[]={"path_follower/camera/main","path_follower/light/light_key","path_follower/object/obj_sphere_medium"};
  for(size_t k=0;k<3;++k) {SDL_Rect row;if(!SceneEditorRenderAuthoringControl(followers[k],&row)) {fprintf(stderr,"Follower not visible: %s\n",followers[k]);capture(editor,"follower_layout_failure.ppm");}assert(SceneEditorRenderAuthoringControl(followers[k],&row));}
  capture(editor,"path_followers_object.ppm");
  /* A changed draft must never travel to another follower. */
  dm4_inspector_control(editor,"path_follower_plan");
  assert(!strcmp(SceneEditorMotionPlanPanelTarget(),"object/obj_sphere_medium"));
  dm4_inspector_control(editor,"plan_speed");authoring_text(editor,"17");
  authoring_control(editor,"path_followers");
  dm4_inspector_control(editor,"path_follower_camera");dm4_inspector_control(editor,"path_follower_plan");
  assert(!strcmp(SceneEditorMotionPlanPanelTarget(),"camera/main"));
  dm4_inspector_control(editor,"plan_apply");
  MotionTimingScheduleRequest saved;
  assert(SceneEditorMotionPlanRead("camera/main",&saved) && saved.max_speed==10);
  assert(!MotionPlansRuntimeActive("object/obj_sphere_medium"));
  dm4_inspector_control(editor,"plan_open");
  dm4_inspector_control(editor,"path_restore_replan");
  assert(!strcmp(SceneEditorMotionPlanPanelTarget(),"camera/main"));
  dm4_inspector_control(editor,"plan_restore");assert(!MotionPlansRuntimeActive("camera/main"));
  authoring_control(editor,"path_followers");
  dm4_inspector_control(editor,"path_follower_light");dm4_inspector_control(editor,"path_follower_plan");
  assert(!strcmp(SceneEditorMotionPlanPanelTarget(),"light/light_key"));
  dm4_inspector_control(editor,"plan_apply");assert(MotionPlansRuntimeActive("light/light_key"));
  assert(!MotionPlansRuntimeActive("camera/main") && !MotionPlansRuntimeActive("object/obj_sphere_medium"));
  dm4_inspector_control(editor,"plan_restore");
  authoring_control(editor,"path_followers");
  dm4_inspector_control(editor,"path_follower_back");
  authoring_control(editor,"path_follower/camera/main");
  assert(SceneEditorRenderAuthoringControl("path_camera_detach",&hidden));
  TimelineTrack selected_track;TimelineRate selected_rate;TimelineRange selected_range;TimelineSample selected_sample;
  assert(SceneEditorTimelineSelectedTrack(&selected_track,&selected_rate,&selected_range,&selected_sample));
  assert(!strcmp(selected_track.target_id,"camera/main") && !strcmp(selected_track.property_id,MOTION_CAMERA_PROGRESS_PROPERTY));
  dm4_inspector_control(editor,"path_camera_timing");assert(!SceneEditorMotionPathPanelActive());
  authoring_control(editor,"paths");assert(SceneEditorMotionPathPanelActive());
  assert(SceneEditorRenderAuthoringControl("path_camera_detach",&hidden));
  assert(!SceneEditorRenderAuthoringControl("path_detach",&hidden));
  dm4_inspector_control(editor,"path_camera_detach");
  assert(SceneEditorMotionPathsRead(&paths));
  assert(!paths.bindings[0].enabled && paths.bindings[1].enabled && paths.bindings[2].enabled);
  capture(editor,"path_followers_camera.ppm");
  char message[256];
  json_object *disk_before=json_object_from_file(scene);assert(disk_before);
  SceneEditorDocumentFailNextForTests(SCENE_DOCUMENT_FAIL_SAVE_SYNC);
  choose_menu(editor,0,0);assert(SceneEditorDocumentIsDirty());
  assert(strstr(SceneEditorChromeActionsSaveError(),"failed to sync"));
  json_object *disk_after=json_object_from_file(scene);assert(disk_after && json_object_equal(disk_before,disk_after));
  json_object_put(disk_before);json_object_put(disk_after);
  assert(SceneEditorMotionPathsRead(&paths) && paths.count==2 && paths.bindings[2].follow_direction);
  choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty() && !*SceneEditorChromeActionsSaveError());
  assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
  assert(SceneEditorMotionPathsRead(&paths) && paths.count==2);
  assert(!strcmp(paths.paths[0].name,"Object approach") && !strcmp(paths.paths[1].name,"Camera orbit"));
  assert(paths.bindings[2].follow_direction && paths.bindings[2].rotation_offset[2]==30);
  fprintf(stderr,"Path usability library PASS: unique names, creation preserves existing route, inline rename, selection, compact controls, typed attachments, hidden controls, camera detach isolation and save/reopen.\n");
}

static void path_light_setup_acceptance(SceneEditor *editor) {
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);authoring_control(editor,"paths");authoring_control(editor,"new_path");authoring_text(editor,"Existing timeline");
  MotionPaths paths;char message[256];assert(SceneEditorMotionPathsRead(&paths));
  assert(SceneEditorMotionPathBindCamera(paths.paths[0].id,true,SceneEditorDocumentRevision(),message,sizeof(message)));
  static TimelineDocument before,after;
  assert(SceneEditorDocumentGetTimeline(&before)==TIMELINE_STATUS_OK);
  authoring_control(editor,"path_followers");dm4_inspector_control(editor,"path_follower_light");dm4_inspector_control(editor,"path_light_attach");
  if(!SceneEditorMotionPathsRead(&paths) || paths.binding_count!=2 || !paths.bindings[1].enabled)
    fprintf(stderr,"Light setup refused: %s (legacy points %d)\n",SceneEditorMotionPathPanelStatus(),sceneSettings.bezierPath.numPoints);
  assert(SceneEditorMotionPathsRead(&paths) && paths.binding_count==2 && paths.bindings[1].enabled);
  assert(SceneEditorDocumentGetTimeline(&after)==TIMELINE_STATUS_OK);
  for(size_t i=0;i<before.track_count;++i) assert(!memcmp(&before.tracks[i],&after.tracks[i],sizeof(TimelineTrack)));
  dm4_inspector_control(editor,"path_light_detach");assert(SceneEditorMotionPathsRead(&paths) && !paths.bindings[1].enabled);
  fprintf(stderr,"Light setup PASS: existing camera timeline preserved, explicit light setup/attach/detach.\n");
}

static void path_usability_review(SceneEditor *editor) {
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);authoring_control(editor,"paths");
  MotionPaths paths;assert(SceneEditorMotionPathsRead(&paths) && paths.count);
  dm2_repair_select_row(editor,paths.paths[0].id);authoring_control(editor,"path_frame_selected");
  SDL_SetWindowSize(editor->window,1024,640);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);SceneEditorSessionRuntimeRender(editor);
  capture(editor,"path_final_shape.ppm");
  authoring_control(editor,"path_followers");dm4_inspector_control(editor,"path_follower_object");
  capture(editor,"path_final_followers.ppm");
}
