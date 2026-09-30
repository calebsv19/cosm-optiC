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
  char message[256];assert(SceneEditorDocumentSave(message,sizeof(message)));
  assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
  assert(SceneEditorMotionPathsRead(&paths) && paths.count==2);
  assert(!strcmp(paths.paths[0].name,"Object approach") && !strcmp(paths.paths[1].name,"Camera orbit"));
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
