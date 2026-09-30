#include "editor/scene_editor_motion_trail.h"
static void dm4_trail_drag(SceneEditor *editor,int cancel) {
  SceneEditorMotionTrail trail;assert(SceneEditorMotionTrailRead("object/obj_sphere_medium",&trail));
  double xyz[3];assert(SceneEditorMotionTrailSample(&trail,30,xyz));
  SceneEditorDigestOverlayProjector projector=dm1_projector();double scale=SceneEditorDocumentWorldScale();int x,y;
  assert(SceneEditorDigestOverlayProjectPoint(&projector,xyz[0]*scale,xyz[1]*scale,xyz[2]*scale,&x,&y));
  unsigned long long rev=SceneEditorDocumentRevision();
  SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;SceneEditorSessionRuntimeHandleEvent(editor,&e);
  e=(SDL_Event){.type=SDL_MOUSEMOTION};e.motion.x=x+25;e.motion.y=y+12;e.motion.state=SDL_BUTTON_LMASK;SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
  if(cancel==1)key(editor,SDLK_ESCAPE);
  if(cancel==2){SDL_Event lost={.type=SDL_WINDOWEVENT};lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;SceneEditorSessionRuntimeHandleEvent(editor,&lost);}
  if(cancel==3){SDL_SetModState(KMOD_ALT);SceneEditorSessionRuntimeHandleEvent(editor,&e);SDL_SetModState(KMOD_NONE);}
  e=(SDL_Event){.type=SDL_MOUSEBUTTONUP};e.button.button=SDL_BUTTON_LEFT;e.button.x=x+25;e.button.y=y+12;SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
  assert(SceneEditorDocumentRevision()==rev+(cancel?0:1));
  SceneEditorMotionTrail after;assert(SceneEditorMotionTrailRead(trail.target,&after));double pos[3];assert(SceneEditorMotionTrailSample(&after,30,pos));
  if(cancel)assert(!memcmp(xyz,pos,sizeof(xyz)));
  else {assert(fabs(xyz[0]-pos[0])+fabs(xyz[1]-pos[1])>1e-5 && xyz[2]==pos[2]);choose_menu(editor,1,0);choose_menu(editor,1,1);}
}
static void dm4_trail(SceneEditor *editor,const char *scene,bool reopen) {
  static TimelineDocument doc,prior;char message[256];SceneEditorMotionTrail trail;
  const char *target="object/obj_sphere_medium";
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
  if(reopen) {
    assert(SceneEditorMotionTrailRead(target,&trail));
    for(size_t i=trail.count;i>0;--i){double xyz[3];int64_t frame=trail.frames[i-1];assert(SceneEditorMotionTrailSample(&trail,frame,xyz));TimelineVec3 actual=dm2_sample("obj_sphere_medium",frame);double scale=SceneEditorDocumentWorldScale();assert(fabs(actual.x-xyz[0]*scale)<1e-8 && fabs(actual.y-xyz[1]*scale)<1e-8 && fabs(actual.z-xyz[2]*scale)<1e-8);}
    fprintf(stderr,"D-M4 trail reopen PASS: reverse marker evaluation parity\n");return;
  }
  assert(SceneEditorObjectTimelineAdd("obj_sphere_medium",message,sizeof(message)));
  assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  size_t selected=SIZE_MAX;
  for(size_t i=0;i<doc.track_count;++i)if(!strcmp(doc.tracks[i].target_id,target)) {
    TimelineTrack *t=&doc.tracks[i];int a=RuntimeObjectTimelineAxis(t->property_id);if(a<0)continue;
    if(a==0)selected=i;t->key_count=0;
    assert(TimelineTrackAddKey(t,0,TimelineValueScalar(a==2?1:0),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
    if(a<2){assert(TimelineTrackAddKey(t,a?60:30,TimelineValueScalar(a?2:3),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);assert(TimelineTrackAddKey(t,119,TimelineValueScalar(a?0:5),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);}
  }
  assert(SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),message,sizeof(message)));
  assert(SceneEditorTimelineSelectTrack(selected));assert(SceneEditorMotionTrailRead(target,&trail));
  assert(trail.count==4 && trail.frames[1]==30 && trail.frames[2]==60);
  double xyz[3];assert(SceneEditorMotionTrailSample(&trail,30,xyz));assert(xyz[0]==3 && xyz[1]==1 && xyz[2]==1);
  /* Existing complete-XYZ and exclusive-owner rules remain enforced. */
  prior=doc;doc.tracks[selected].enabled=false;
  unsigned long long initial=SceneEditorDocumentRevision();
  assert(!SceneEditorDocumentSetTimeline(&doc,initial,message,sizeof(message)) && SceneEditorDocumentRevision()==initial);doc=prior;
  /* Fill one axis to capacity; no other axis may be partially written. */
  doc.range.frame_count=300;doc.tracks[selected].key_count=0;
  for(int i=0;i<128;++i)assert(TimelineTrackAddKey(&doc.tracks[selected],2*i,TimelineValueScalar(i*.01),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
  assert(SceneEditorDocumentSetTimeline(&doc,initial,message,sizeof(message)));
  initial=SceneEditorDocumentRevision();assert(!SceneEditorMotionTrailSetKey(target,31,(double[]){2,3,4},initial,message,sizeof(message)) && SceneEditorDocumentRevision()==initial);
  choose_menu(editor,1,0);assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);assert(!memcmp(&doc,&prior,sizeof(doc)));
  prior=doc;unsigned long long rev=SceneEditorDocumentRevision();
  assert(!SceneEditorMotionTrailSetKey(target,30,(double[]){9,9,9},rev-1,message,sizeof(message)));assert(SceneEditorDocumentRevision()==rev);
  assert(!SceneEditorMotionTrailSetKey(target,30,(double[]){9,NAN,9},rev,message,sizeof(message)));assert(SceneEditorDocumentRevision()==rev);
  assert(SceneEditorMotionTrailSetKey(target,30,(double[]){3,1,2},rev,message,sizeof(message)));
  assert(SceneEditorDocumentRevision()==rev+1);assert(SceneEditorMotionTrailRead(target,&trail));
  for(int a=0;a<3;++a){bool found=false;for(size_t k=0;k<trail.axes[a].key_count;++k)if(trail.axes[a].keys[k].frame==30)found=true;assert(found);}
  choose_menu(editor,1,0);assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);assert(!memcmp(&doc,&prior,sizeof(doc)));
  choose_menu(editor,1,1);assert(SceneEditorMotionTrailRead(target,&trail));
  SceneEditorSessionRuntimeRender(editor);authoring_control(editor,"frame_xyz_trail");
  SceneEditorSessionRuntimeRender(editor);dm4_trail_drag(editor,1);dm4_trail_drag(editor,2);dm4_trail_drag(editor,3);dm4_trail_drag(editor,0);
  capture(editor,"dm4_trail.ppm");
  SDL_SetWindowSize(editor->window,1024,640);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);authoring_control(editor,"frame_xyz_trail");SceneEditorSessionRuntimeRender(editor);capture(editor,"dm4_trail_compact.ppm");
  const int frames[]={0,15,30,45,60,90,119};json_object *rows=json_object_new_array();
  for(size_t i=0;i<sizeof(frames)/sizeof(frames[0]);++i){TimelineVec3 actual=dm2_sample("obj_sphere_medium",frames[i]);json_object *row=json_object_new_array();json_object_array_add(row,json_object_new_double(actual.x));json_object_array_add(row,json_object_new_double(actual.y));json_object_array_add(row,json_object_new_double(actual.z));json_object_array_add(rows,row);}
  assert(json_object_to_file_ext("dm4_trail_expected.json",rows,JSON_C_TO_STRING_PRETTY)==0);json_object_put(rows);
  assert(SceneEditorDocumentSave(message,sizeof(message)));assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
  assert(SceneEditorMotionTrailRead(target,&trail));
  fprintf(stderr,"D-M4 trail PASS: union key times, constant axis, atomic XYZ insertion, stale/invalid refusal, exact undo/redo and reopen\n");
}
