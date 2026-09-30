static void dm4_combined(SceneEditor *editor,const char *scene,bool reopen) {
  const int frames[]={0,15,23,40,60,90,119};char message[256];
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
  if(!reopen) {
    MotionPaths paths;assert(SceneEditorMotionPathsRead(&paths));
    for(size_t i=0;i<paths.count;++i)for(size_t k=0;k<paths.paths[i].count;++k)
      assert(MotionPathSetHandleMode(&paths.paths[i].points[k],MOTION_HANDLE_LINKED));
    assert(SceneEditorMotionPathsSet(&paths,SceneEditorDocumentRevision(),message,sizeof(message)));
    static TimelineDocument doc;assert(SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK);
    const int times[]={0,23,60,119};
    for(size_t i=0;i<doc.track_count;++i) {
      TimelineTrack *t=&doc.tracks[i];if(!t->enabled)continue;
      bool camera=!strcmp(t->property_id,MOTION_CAMERA_PROGRESS_PROPERTY),light=!strcmp(t->property_id,MOTION_LIGHT_PROGRESS_PROPERTY),object=!strcmp(t->property_id,MOTION_PROGRESS_PROPERTY);
      if(!camera && !light && !object)continue;
      double values[]={0,.2,.65,1};if(light){values[1]=.3;values[2]=.3;}if(object){values[1]=.6;values[2]=.3;}
      t->key_count=0;
      for(int k=0;k<4;++k){assert(TimelineTrackAddKey(t,times[k],TimelineValueScalar(values[k]),TIMELINE_INTERPOLATION_CUBIC_BEZIER)==TIMELINE_STATUS_OK);t->keys[k].tangent_mode=TIMELINE_TANGENT_AUTO_CLAMPED;}
      assert(TimelineTrackRecomputeTangents(t)==TIMELINE_STATUS_OK);
    }
    assert(SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),message,sizeof(message)));
    assert(SceneEditorDocumentSave(message,sizeof(message)));
  }
  json_object *rows=reopen?json_object_from_file("dm4_combined_expected.json"):json_object_new_array();assert(rows);
  for(int j=0;j<7;++j) {
    int i=reopen?6-j:j;RayEvaluatedSceneSnapshot sample=dm3_complete_sample(frames[i],0);dm3_focus_check(&sample.camera);
    json_object *row=dm3_complete_row(&sample,true);
    if(reopen){json_object *prior=json_object_array_get_idx(rows,i);for(int k=0;k<13;++k)assert(fabs(json_object_get_double(json_object_array_get_idx(row,k))-json_object_get_double(json_object_array_get_idx(prior,k)))<1e-8);json_object_put(row);}
    else json_object_array_add(rows,row);
  }
  if(!reopen)assert(json_object_to_file_ext("dm4_combined_expected.json",rows,JSON_C_TO_STRING_PRETTY)==0);
  json_object_put(rows);(void)scene;
  fprintf(stderr,"D-M4 combined %s PASS: linked routes, clamped mesh reversal/light hold/camera motion, focus/FOV/intensity and seven sampled poses\n",reopen?"reopen":"prepare");
}
