/* Native point movement owns one retained command per completed gesture. */
static void path_gizmo_acceptance(SceneEditor *editor) {
  choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);
  authoring_control(editor,"paths");authoring_control(editor,"new_path");
  authoring_text(editor,"Gizmo route");key(editor,SDLK_ESCAPE);
  authoring_control(editor,"path_next");
  const char *axes[]={"path_gizmo_x","path_gizmo_y","path_gizmo_z"};
  for(int view=0;view<2;++view) {
    SceneEditorDigestOverlayNavState nav=*SceneEditorGetViewportNavState();
    nav.orbit_yaw_deg=view?125:35;nav.orbit_pitch_deg=view?20:40;
    SceneEditorRestoreViewportNav(&nav);SceneEditorSessionRuntimeRender(editor);
    for(int axis=0;axis<3;++axis) {
      MotionPaths before,after;assert(SceneEditorMotionPathsRead(&before));
      unsigned long long revision=SceneEditorDocumentRevision();
      SDL_Rect h;assert(SceneEditorRenderAuthoringControl(axes[axis],&h));
      int x=h.x+h.w/2,y=h.y+h.h/2,px,py;
      SceneEditorDigestOverlayProjector projector=dm1_projector();double scale=SceneEditorDocumentWorldScale();
      double *point=before.paths[0].points[1].position;
      assert(SceneEditorDigestOverlayProjectPoint(&projector,point[0]*scale,point[1]*scale,point[2]*scale,&px,&py));
      double length=hypot(x-px,y-py);assert(length>8);
      int dx=(int)lround(25*(x-px)/length),dy=(int)lround(25*(y-py)/length);
      SDL_Event e={.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
      SceneEditorSessionRuntimeHandleEvent(editor,&e);
      e=(SDL_Event){.type=SDL_MOUSEMOTION};e.motion.state=SDL_BUTTON_LMASK;e.motion.x=x+dx;e.motion.y=y+dy;
      SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
      assert(SceneEditorDocumentRevision()==revision);
      assert(SceneEditorMotionPathsRead(&after) && !memcmp(&before,&after,sizeof(before)));
      e=(SDL_Event){.type=SDL_MOUSEBUTTONUP};e.button.button=SDL_BUTTON_LEFT;e.button.x=x+dx;e.button.y=y+dy;
      SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
      assert(SceneEditorDocumentRevision()==revision+1);
      assert(SceneEditorMotionPathsRead(&after));
      for(int k=0;k<3;++k) {
        double delta=after.paths[0].points[1].position[k]-point[k];
        assert(k==axis?delta>1e-8:delta==0);
      }
      choose_menu(editor,1,0);
      assert(SceneEditorMotionPathsRead(&after) && !memcmp(&before,&after,sizeof(before)));
      /* Escape drops the same preview without modifying source/history. */
      revision=SceneEditorDocumentRevision();
      e=(SDL_Event){.type=SDL_MOUSEBUTTONDOWN};e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
      SceneEditorSessionRuntimeHandleEvent(editor,&e);
      e=(SDL_Event){.type=SDL_MOUSEMOTION};e.motion.state=SDL_BUTTON_LMASK;e.motion.x=x+dx;e.motion.y=y+dy;
      SceneEditorSessionRuntimeHandleEvent(editor,&e);key(editor,SDLK_ESCAPE);
      e=(SDL_Event){.type=SDL_MOUSEBUTTONUP};e.button.button=SDL_BUTTON_LEFT;e.button.x=x+dx;e.button.y=y+dy;
      SceneEditorSessionRuntimeHandleEvent(editor,&e);SceneEditorSessionRuntimeRender(editor);
      assert(SceneEditorDocumentRevision()==revision);
      assert(SceneEditorMotionPathsRead(&after) && !memcmp(&before,&after,sizeof(before)));
    }
  }
  /* A click/release without movement must not add history. */
  unsigned long long revision=SceneEditorDocumentRevision();
  authoring_control(editor,"path_gizmo_x");
  assert(SceneEditorDocumentRevision()==revision);
  capture(editor,"path_point_gizmo.ppm");
  SDL_Rect xaxis;assert(SceneEditorRenderAuthoringControl("path_gizmo_x",&xaxis));
  dm2_navigation_drag(editor,xaxis.x,xaxis.y,SDL_BUTTON_LEFT,KMOD_ALT);
  dm2_navigation_drag(editor,xaxis.x,xaxis.y,SDL_BUTTON_RIGHT,KMOD_NONE);
  fprintf(stderr,"Path gizmo PASS: XYZ isolation in two views, transient preview, single retained command, undo, Escape and orbit/pan priority.\n");
}
