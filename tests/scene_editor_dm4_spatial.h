static void dm4_inspector_control(SceneEditor *editor,const char *name) {
  SceneEditorPaneLayout layout;assert(SceneEditorGetPaneLayout(&layout));SDL_Rect rect;
  SDL_Event e={.type=SDL_MOUSEWHEEL};e.wheel.mouseX=layout.right_content_rect.x+20;e.wheel.mouseY=layout.right_content_rect.y+30;
  e.wheel.y=100;SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
  for(int i=0;i<30 && !SceneEditorRenderAuthoringControl(name,&rect);++i) {
    e.wheel.y=-1;SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
  }
  assert(SceneEditorRenderAuthoringControl(name,&rect));click(editor,rect);
}
static void dm4_handle_drag(SceneEditor *editor, bool cancel) {
  MotionPaths paths;assert(SceneEditorMotionPathsRead(&paths));
  MotionPathPoint before=paths.paths[0].points[0];
  authoring_control(editor,"path_frame_selected");
  SceneEditorDigestOverlayProjector projector=dm1_projector();
  double scale=SceneEditorDocumentWorldScale();int x,y;
  assert(SceneEditorDigestOverlayProjectPoint(&projector,(before.position[0]+before.outgoing[0])*scale,
      (before.position[1]+before.outgoing[1])*scale,(before.position[2]+before.outgoing[2])*scale,&x,&y));
  unsigned long long rev=SceneEditorDocumentRevision();
  SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
  SceneEditorSessionRuntimeHandleEvent(editor,&e);
  e=(SDL_Event){.type=SDL_MOUSEMOTION};e.motion.x=x+24;e.motion.y=y+15;e.motion.state=SDL_BUTTON_LMASK;
  SceneEditorSessionRuntimeHandleEvent(editor,&e);
  if(cancel)key(editor,SDLK_ESCAPE);
  e=(SDL_Event){.type=SDL_MOUSEBUTTONUP};e.button.button=SDL_BUTTON_LEFT;e.button.x=x+24;e.button.y=y+15;
  SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
  assert(SceneEditorDocumentRevision()==rev+(cancel?0:1));assert(SceneEditorMotionPathsRead(&paths));
  MotionPathPoint after=paths.paths[0].points[0];
  if(cancel)assert(!memcmp(&before,&after,sizeof(before)));
  else {
    assert(fabs(after.outgoing[0]-before.outgoing[0])>1e-5);
    assert(fabs(after.incoming[0]*after.outgoing[1]-after.incoming[1]*after.outgoing[0])<1e-9);
    choose_menu(editor,1,0);
  }
}
/* Spatial policy, compatibility, retained transaction and GUI mode proof. */
static void dm4_spatial(SceneEditor *editor, bool reopen) {
  MotionPaths paths;char message[256],id[64];
  assert(SceneEditorMotionPathsRead(&paths));
  if(reopen) {
    assert(paths.count==1 && paths.paths[0].points[0].handle_mode==MOTION_HANDLE_CORNER);
    assert(paths.paths[0].points[1].handle_mode==MOTION_HANDLE_LINKED);
    for(int k=0;k<3;++k) assert(paths.paths[0].points[0].incoming[k]==0 && paths.paths[0].points[0].outgoing[k]==0);
    fprintf(stderr,"D-M4 spatial reopen PASS\n");return;
  }
  MotionPathPoint p={.incoming={-2,0,0},.outgoing={1,0,0}};
  assert(MotionPathSetHandleMode(&p,MOTION_HANDLE_LINKED));
  assert(MotionPathEditHandle(&p,false,(double[]){0,3,0}));
  assert(fabs(p.incoming[1]+2)<1e-12 && p.incoming[0]==0);
  assert(MotionPathEditHandle(&p,true,(double[]){0,0,-4}));
  assert(fabs(p.outgoing[2]-3)<1e-12);
  /* A three-point cubic verifies unit tangent continuity independently of
   * parameter speed, which may differ with unequal handle lengths. */
  MotionPath curve={.count=3};
  curve.points[0].position[0]=-3;curve.points[2].position[0]=3;
  curve.points[0].outgoing[0]=1;curve.points[2].incoming[0]=-1;
  curve.points[1]=p;double l[3],c[3],r[3];
  MotionPathPointAt(&curve,1-1e-6,l);MotionPathPointAt(&curve,1,c);MotionPathPointAt(&curve,1+1e-6,r);
  double dot=0,ll=0,rr=0;for(int k=0;k<3;++k){double a=c[k]-l[k],b=r[k]-c[k];dot+=a*b;ll+=a*a;rr+=b*b;}
  assert(dot/sqrt(ll*rr)>.999999);
  MotionPathPoint before=p;
  assert(!MotionPathEditHandle(&p,false,(double[]){NAN,0,0}));assert(!memcmp(&p,&before,sizeof(p)));
  assert(MotionPathSetHandleMode(&p,MOTION_HANDLE_INDEPENDENT));
  assert(MotionPathEditHandle(&p,false,(double[]){2,1,0}));assert(p.incoming[2]==-4);
  assert(MotionPathSetHandleMode(&p,MOTION_HANDLE_CORNER));
  assert(MotionPathEditHandle(&p,false,(double[]){1,0,0}));assert(p.handle_mode==MOTION_HANDLE_INDEPENDENT && p.incoming[2]==0);
  memset(&p,0,sizeof(p));assert(MotionPathSetHandleMode(&p,MOTION_HANDLE_LINKED));
  assert(MotionPathEditHandle(&p,true,(double[]){0,-1,0}));assert(p.outgoing[1]==1);
  assert(SceneEditorMotionPathCreate("M4 spatial",(double[]){0,0,1},3,SceneEditorDocumentRevision(),id,sizeof(id),message,sizeof(message)));
  assert(SceneEditorMotionPathsRead(&paths));
  /* Old records omit policy: loading must retain every handle exactly. */
  json_object *a=json_object_new_object(),*j=MotionPathsToJson(&paths);
  json_object_object_add(a,"motion_paths",j);
  json_object *ps=NULL,*pts=NULL;json_object_object_get_ex(j,"paths",&ps);
  json_object_object_get_ex(json_object_array_get_idx(ps,0),"points",&pts);
  json_object_object_del(json_object_array_get_idx(pts,0),"handle_mode");
  MotionPaths old;assert(MotionPathsParse(a,&old,message,sizeof(message)));
  assert(!memcmp(paths.paths[0].points,old.paths[0].points,sizeof(paths.paths[0].points)));
  json_object_object_add(json_object_array_get_idx(pts,0),"handle_mode",json_object_new_string("unknown"));
  assert(!MotionPathsParse(a,&old,message,sizeof(message)));json_object_put(a);
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);SceneEditorMotionPathPanelSelect(true);
  SceneEditorSessionRuntimeRender(editor);
  dm4_inspector_control(editor,"path_handle_mode");
  assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].points[0].handle_mode==MOTION_HANDLE_LINKED);
  choose_menu(editor,1,0);assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].points[0].handle_mode==MOTION_HANDLE_INDEPENDENT);
  choose_menu(editor,1,1);assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].points[0].handle_mode==MOTION_HANDLE_LINKED);
  dm4_handle_drag(editor,true);dm4_handle_drag(editor,false);
  dm4_inspector_control(editor,"path_out_y");authoring_text(editor,"2");
  assert(SceneEditorMotionPathsRead(&paths));
  assert(paths.paths[0].points[0].outgoing[1]==2);
  double *in=paths.paths[0].points[0].incoming,*out=paths.paths[0].points[0].outgoing;
  assert(fabs(in[0]*out[1]-in[1]*out[0])<1e-12 && in[1]<0);
  dm4_inspector_control(editor,"path_handle_mode");
  assert(SceneEditorMotionPathsRead(&paths) && paths.paths[0].points[0].handle_mode==MOTION_HANDLE_CORNER);
  assert(MotionPathSetHandleMode(&paths.paths[0].points[1],MOTION_HANDLE_LINKED));
  assert(SceneEditorMotionPathsSet(&paths,SceneEditorDocumentRevision(),message,sizeof(message)));
  /* Splitting a straight segment must preserve its dormant corner handles. */
  paths.paths[0].points[0].linear=true;
  assert(SceneEditorMotionPathsSet(&paths,SceneEditorDocumentRevision(),message,sizeof(message)));
  SceneEditorSessionRuntimeRender(editor);dm4_inspector_control(editor,"path_add");
  MotionPaths split;assert(SceneEditorMotionPathsRead(&split) && split.paths[0].count==3);
  for(int k=0;k<3;++k)assert(split.paths[0].points[0].incoming[k]==0 && split.paths[0].points[0].outgoing[k]==0);
  dm4_inspector_control(editor,"path_remove");assert(SceneEditorMotionPathsRead(&split) && split.paths[0].count==2);
  choose_menu(editor,1,0);choose_menu(editor,1,0);
  assert(SceneEditorMotionPathsRead(&split) && split.paths[0].count==2);
  assert(!memcmp(&split.paths[0],&paths.paths[0],sizeof(MotionPath)));
  SDL_SetWindowSize(editor->window,1024,640);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);
  dm4_inspector_control(editor,"path_handle_mode");choose_menu(editor,1,0);
  assert(SceneEditorDocumentSave(message,sizeof(message)));
  fprintf(stderr,"D-M4 spatial PASS: policy math, zero/invalid vectors, legacy/invalid JSON, GUI mode, undo/redo and saved modes\n");
}
