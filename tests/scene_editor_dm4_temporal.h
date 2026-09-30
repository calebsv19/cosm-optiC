static double dm4_temporal_value(const TimelineTrack *t,int frame,unsigned sub,double *derivative) {
  TimelineEvaluationContext context;TimelineEvaluationResult value;
  assert(TimelineEvaluationContextBuild((TimelineRate){24,1},(TimelineRange){0,121},(TimelineSample){frame,sub,10000},&context)==TIMELINE_STATUS_OK);
  assert(TimelineTrackEvaluate(t,&context,&value)==TIMELINE_STATUS_OK);
  if(derivative)*derivative=value.derivative_per_frame;
  return value.value.as.scalar;
}
static void dm4_temporal(SceneEditor *editor) {
  (void)editor;TimelineTrack t;static TimelineDocument doc,parsed;
  assert(TimelineTrackInit(&t,"m4","camera/main","camera/route_progress",TIMELINE_VALUE_SCALAR)==TIMELINE_STATUS_OK);
  assert(TimelineTrackSetUnit(&t,TIMELINE_UNIT_UNITLESS)==TIMELINE_STATUS_OK);
  assert(TimelineTrackAddKey(&t,0,TimelineValueScalar(0),TIMELINE_INTERPOLATION_CUBIC_BEZIER)==TIMELINE_STATUS_OK);
  assert(TimelineTrackAddKey(&t,10,TimelineValueScalar(.4),TIMELINE_INTERPOLATION_CUBIC_BEZIER)==TIMELINE_STATUS_OK);
  assert(TimelineTrackAddKey(&t,120,TimelineValueScalar(1),TIMELINE_INTERPOLATION_CUBIC_BEZIER)==TIMELINE_STATUS_OK);
  for(int i=0;i<3;++i)t.keys[i].tangent_mode=TIMELINE_TANGENT_AUTO_SMOOTH;
  assert(TimelineTrackRecomputeTangents(&t)==TIMELINE_STATUS_OK);
  assert(TimelineTrackValidate(&t,&(TimelineRange){0,121})==TIMELINE_STATUS_OK);
  double left,right;dm4_temporal_value(&t,9,9999,&left);dm4_temporal_value(&t,10,1,&right);
  assert(left>.001 && fabs(left-right)<1e-6);
  bool overshoot=false;for(int f=10;f<120;++f)if(dm4_temporal_value(&t,f,0,NULL)>1)overshoot=true;
  assert(overshoot); /* Smooth is intentionally not the clamped policy. */
  TimelinePropertyRegistry registry;assert(TimelinePropertyRegistryInitFoundationDefaults(&registry)==TIMELINE_STATUS_OK);
  assert(TimelinePropertyRegistryValidateTrack(&registry,&t,&(TimelineRange){0,121})==TIMELINE_STATUS_VALUE_OUT_OF_RANGE);
  for(int i=0;i<3;++i)t.keys[i].tangent_mode=TIMELINE_TANGENT_AUTO_CLAMPED;
  assert(TimelineTrackRecomputeTangents(&t)==TIMELINE_STATUS_OK);
  assert(TimelinePropertyRegistryValidateTrack(&registry,&t,&(TimelineRange){0,121})==TIMELINE_STATUS_OK);
  double prev=-1;for(int f=0;f<1200;++f){double v=dm4_temporal_value(&t,f/10,(f%10)*1000,NULL);assert(v>=prev-1e-10 && v>=0 && v<=1);prev=v;}
  t.keys[2].value.as.scalar=.1;assert(TimelineTrackRecomputeTangents(&t)==TIMELINE_STATUS_OK);
  assert(t.keys[1].incoming_value_offset==0 && t.keys[1].outgoing_value_offset==0);
  for(int f=0;f<120;++f){double v=dm4_temporal_value(&t,f,0,NULL);assert(v>=0 && v<=.4+1e-10);}
  t.keys[1].tangent_mode=TIMELINE_TANGENT_FLAT;assert(TimelineTrackRecomputeTangents(&t)==TIMELINE_STATUS_OK);
  dm4_temporal_value(&t,10,1,&right);assert(fabs(right)<1e-6);
  assert(TimelineTrackSetScalarTemporalHandles(&t,1,-2,-.1,5,.2)==TIMELINE_STATUS_OK);
  assert(t.keys[1].tangent_mode==TIMELINE_TANGENT_BROKEN);
  assert(TimelineTrackRecomputeTangents(&t)==TIMELINE_STATUS_OK);assert(t.keys[1].outgoing_value_offset==.2);
  TimelineDocumentInit(&doc,(TimelineRate){24,1},(TimelineRange){0,121});
  assert(TimelineDocumentAddTrack(&doc,&t)==TIMELINE_STATUS_OK);
  json_object *j=SceneTimelineDocumentToJson(&doc);assert(j);
  assert(SceneTimelineDocumentFromJson(j,&parsed)==TIMELINE_STATUS_OK);
  assert(!memcmp(&doc.tracks[0],&parsed.tracks[0],sizeof(t)));
  json_object *tracks=NULL,*keys=NULL;assert(json_object_object_get_ex(j,"tracks",&tracks));
  assert(json_object_object_get_ex(json_object_array_get_idx(tracks,0),"keys",&keys));
  json_object_object_add(json_object_array_get_idx(keys,0),"tangent_mode",json_object_new_string("unknown"));
  assert(SceneTimelineDocumentFromJson(j,&parsed)!=TIMELINE_STATUS_OK);json_object_put(j);
  /* A manual neighbor survives recomputation beside an automatic key. */
  double manual=t.keys[1].outgoing_value_offset;
  t.keys[0].tangent_mode=TIMELINE_TANGENT_AUTO_CLAMPED;t.keys[0].value.as.scalar=.05;
  assert(TimelineTrackRecomputeTangents(&t)==TIMELINE_STATUS_OK && t.keys[1].outgoing_value_offset==manual);
  fprintf(stderr,"D-M4 temporal math PASS: nonuniform smooth pass-through, clamped overshoot/reversal, flat stop, broken edits, saved policies\n");
}

static void dm4_temporal_ui(SceneEditor *editor,const char *scene,bool reopen) {
  static TimelineDocument doc;char message[256];size_t at=0;
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
  assert(SceneEditorTimelineActivate());assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  if(!reopen) {
    TimelineTrack t;assert(TimelineTrackInit(&t,"m4-yaw","camera/main","camera/yaw",TIMELINE_VALUE_SCALAR)==TIMELINE_STATUS_OK);
    assert(TimelineTrackSetUnit(&t,TIMELINE_UNIT_RADIANS)==TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&t,0,TimelineValueScalar(0),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&t,10,TimelineValueScalar(.4),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
    assert(TimelineTrackAddKey(&t,119,TimelineValueScalar(1),TIMELINE_INTERPOLATION_LINEAR)==TIMELINE_STATUS_OK);
    at=doc.track_count;assert(TimelineDocumentAddTrack(&doc,&t)==TIMELINE_STATUS_OK);
    assert(SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),message,sizeof(message)));
  } else {while(at<doc.track_count && strcmp(doc.tracks[at].track_id,"m4-yaw"))++at;assert(at<doc.track_count);}
  assert(SceneEditorTimelineSelectTrack(at));
  if(reopen) {
    assert(doc.tracks[at].keys[1].tangent_mode==TIMELINE_TANGENT_AUTO_CLAMPED);
    for(int frame=119;frame>=0;frame-=7) {
      assert(SceneEditorTimelineSeek(frame));RayEvaluatedSceneSnapshot snapshot;assert(SceneEditorTimelineCopyEvaluated(&snapshot));
      assert(fabs(snapshot.camera.yaw_radians-dm4_temporal_value(&doc.tracks[at],frame,0,NULL))<1e-8);
    }
    fprintf(stderr,"D-M4 temporal reopen PASS: saved policies and reverse-seek evaluated yaw parity\n");return;
  }
  assert(SceneEditorTimelineSelectAllKeys());SceneEditorSessionRuntimeRender(editor);
  dock_control(editor,"interpolation");SDL_Rect rect;assert(SceneEditorTimelineControl("interpolation",&rect));
  rect.y-=168;rect.y+=3*24;rect.h=24;click(editor,rect); /* Auto Smooth */
  assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  for(int i=0;i<3;++i)assert(doc.tracks[at].keys[i].tangent_mode==TIMELINE_TANGENT_AUTO_SMOOTH);
  assert(SceneEditorTimelineSelectKey(10,false));
  assert(SceneEditorTimelineMoveSelectedKeys(30));assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  double incoming=doc.tracks[at].keys[1].incoming_value_offset/doc.tracks[at].keys[1].incoming_frame_offset;
  double outgoing=doc.tracks[at].keys[1].outgoing_value_offset/doc.tracks[at].keys[1].outgoing_frame_offset;
  assert(fabs(incoming-outgoing)<1e-12 && incoming>0);
  assert(SceneEditorTimelineSeek(60));assert(SceneEditorTimelineSetKey(.7));
  assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
  assert(doc.tracks[at].keys[2].tangent_mode==TIMELINE_TANGENT_AUTO_SMOOTH);
  assert(SceneEditorTimelineSelectKey(60,false));assert(SceneEditorTimelineDeleteSelectedKeys());
  choose_menu(editor,1,0);assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[at].key_count==4);
  choose_menu(editor,1,1);assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[at].key_count==3);
  assert(SceneEditorTimelineSelectAllKeys());assert(SceneEditorTimelineSelectedTangentMode(TIMELINE_TANGENT_AUTO_CLAMPED));
  assert(SceneEditorDocumentSave(message,sizeof(message)));
  assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
  assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK && doc.tracks[at].keys[1].tangent_mode==TIMELINE_TANGENT_AUTO_CLAMPED);
  fprintf(stderr,"D-M4 temporal UI PASS: menu policy, retime, insertion/deletion, grouped undo/redo, save/reopen\n");
}
