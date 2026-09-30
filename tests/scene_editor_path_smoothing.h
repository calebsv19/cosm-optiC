static void path_smoothing_acceptance(SceneEditor *editor,const char *scene) {
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
  authoring_control(editor,"paths");authoring_control(editor,"new_path");authoring_text(editor,"Smooth route");
  key(editor,SDLK_ESCAPE);dm4_inspector_control(editor,"path_add");
  MotionPaths before,after;assert(SceneEditorMotionPathsRead(&before));
  MotionPath *p=&before.paths[0];assert(p->count==3);
  const double positions[3][3]={{0,0,0},{2,0,0},{3,3,0}};
  for(int i=0;i<3;++i) {
    memcpy(p->points[i].position,positions[i],sizeof(positions[i]));
    assert(MotionPathSetHandleMode(&p->points[i],MOTION_HANDLE_CORNER));p->points[i].linear=true;
  }
  char message[256];assert(SceneEditorMotionPathsSet(&before,SceneEditorDocumentRevision(),message,sizeof(message)));
  SceneEditorSessionRuntimeRender(editor);
  unsigned long long revision=SceneEditorDocumentRevision();key(editor,SDLK_l);SceneEditorSessionRuntimeRender(editor);
  assert(SceneEditorDocumentRevision()==revision+1 && SceneEditorMotionPathsRead(&after));
  const MotionPathPoint *middle=&after.paths[0].points[1];
  assert(middle->handle_mode==MOTION_HANDLE_LINKED);
  assert(!after.paths[0].points[0].linear && !middle->linear);
  double dot=0;
  for(int k=0;k<3;++k) {
    dot+=middle->incoming[k]*middle->outgoing[k];
    assert(middle->position[k]==positions[1][k]);
  }
  assert(dot<0);
  assert(fabs(middle->incoming[0]*middle->outgoing[1]-middle->incoming[1]*middle->outgoing[0])<1e-12);
  assert(hypot(middle->incoming[0],middle->incoming[1])>0);
  choose_menu(editor,1,0);assert(SceneEditorMotionPathsRead(&after) && !memcmp(&before,&after,sizeof(before)));
  dm4_inspector_control(editor,"path_smooth");assert(SceneEditorMotionPathsRead(&after));
  MotionPaths linked=after;
  dm4_inspector_control(editor,"path_independent");assert(SceneEditorMotionPathsRead(&after));
  assert(after.paths[0].points[1].handle_mode==MOTION_HANDLE_INDEPENDENT);
  assert(!memcmp(after.paths[0].points[1].incoming,linked.paths[0].points[1].incoming,3*sizeof(double)));
  dm4_inspector_control(editor,"path_corner");assert(SceneEditorMotionPathsRead(&after));
  assert(after.paths[0].points[1].incoming[0]==0 && after.paths[0].points[1].outgoing[1]==0);
  key(editor,SDLK_l);SceneEditorSessionRuntimeRender(editor);assert(SceneEditorMotionPathsRead(&after));
  assert(after.paths[0].points[1].handle_mode==MOTION_HANDLE_LINKED);
  revision=SceneEditorDocumentRevision();
  authoring_control(editor,"path_followers");key(editor,SDLK_l);assert(SceneEditorDocumentRevision()==revision);
  dm4_inspector_control(editor,"path_follower_back");
  authoring_control(editor,"path_name");key(editor,SDLK_l);
  assert(SceneEditorDocumentRevision()==revision);key(editor,SDLK_ESCAPE);
  MotionPath endpoint=before.paths[0];assert(MotionPathSmoothPoint(&endpoint,0));
  assert(endpoint.points[0].outgoing[0]>0 && !endpoint.points[0].linear);
  MotionPath collapsed=before.paths[0];
  for(size_t i=0;i<collapsed.count;++i) memset(collapsed.points[i].position,0,3*sizeof(double));
  MotionPath unchanged=collapsed;assert(!MotionPathSmoothPoint(&collapsed,1));assert(!memcmp(&collapsed,&unchanged,sizeof(collapsed)));
  assert(SceneEditorDocumentSave(message,sizeof(message)));assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
  MotionPaths reopened;assert(SceneEditorMotionPathsRead(&reopened) && !memcmp(&after,&reopened,sizeof(after)));
  SceneEditorSessionRuntimeRender(editor);capture(editor,"path_smooth_controls.ppm");
  fprintf(stderr,"Path smoothing PASS: L and explicit choices, collapsed handles, adjacent curves, endpoint, degenerate refusal, independent preservation, undo, text focus and reopen.\n");
}
