/* Native consumer keeps a completion while later jobs and documents replace owners. */
static void l3_render_wait(void){for(int i=0;i<3000 && SceneEditorRenderActive();i++){SceneEditorRenderPoll();SDL_Delay(10);}assert(!SceneEditorRenderActive());}
static void l3_render_acceptance(SceneEditor *editor,const char *scene){
 char message[1024];SceneEditorRenderOptions small={.width=32,.height=24,.temporal_frames=1,.frame=0,.integrator=RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT};
 assert(SceneEditorRenderStart("l3-held",&small,SceneEditorDocumentRevision(),message,sizeof(message)));l3_render_wait();
 json_object *held=SceneEditorRenderJobRead("l3-held",true,message,sizeof(message));assert(held);assert(json_object_get_boolean(authoring_t1_member(held,"artifact_integrity")));
 char *bytes=strdup(json_object_to_json_string(held));assert(bytes);
 assert(!SceneEditorRenderStart("l3-held",&small,SceneEditorDocumentRevision(),message,sizeof(message)));
 assert(SceneEditorRenderStart("l3-next",&small,SceneEditorDocumentRevision(),message,sizeof(message)));l3_render_wait();
 assert(!strcmp(bytes,json_object_to_json_string(held)));
 /* Terminal receipt failure cannot turn a committed image into a failed render. */
 assert(SceneEditorRenderStart("l3-receipt-fail",&small,SceneEditorDocumentRevision(),message,sizeof(message)));
 assert(rename(".optic-renders/l3-receipt-fail/job.json",".optic-renders/l3-receipt-fail/prior.json")==0);
 assert(mkdir(".optic-renders/l3-receipt-fail/job.json",0700)==0);l3_render_wait();
 json_object *pending=SceneEditorRenderJobRead("l3-receipt-fail",true,message,sizeof(message));assert(pending);
 assert(!strcmp(json_object_get_string(authoring_t1_member(pending,"state")),"completed"));
 assert(json_object_get_boolean(authoring_t1_member(pending,"artifact_integrity")));json_object_put(pending);
 assert(!SceneEditorRenderStart("l3-must-refuse",&small,SceneEditorDocumentRevision(),message,sizeof(message)));
 assert(rmdir(".optic-renders/l3-receipt-fail/job.json")==0);
 assert(rename(".optic-renders/l3-receipt-fail/prior.json",".optic-renders/l3-receipt-fail/job.json")==0);SceneEditorRenderPoll();
 pending=json_object_from_file(".optic-renders/l3-receipt-fail/job.json");assert(pending && !strcmp(json_object_get_string(authoring_t1_member(pending,"state")),"completed"));json_object_put(pending);
 puts("PASS native committed image survives terminal receipt failure and same-ID recovery");
 /* Publication refusal is observed after admission, while prior completion is held. */
 assert(SceneEditorRenderStart("l3-output-fail",&small,SceneEditorDocumentRevision(),message,sizeof(message)));
 assert(mkdir(".optic-renders/l3-output-fail/frame.bmp",0700)==0);l3_render_wait();
 json_object *failed=SceneEditorRenderJobRead("l3-output-fail",true,message,sizeof(message));assert(failed);assert(!strcmp(json_object_get_string(authoring_t1_member(failed,"state")),"failed"));assert(!json_object_get_boolean(authoring_t1_member(failed,"artifact_integrity")));json_object_put(failed);
 assert(!strcmp(bytes,json_object_to_json_string(held)));
 char second[PATH_MAX];assert(getcwd(second,sizeof(second)));strcat(second,"/second.json");
 assert(SceneEditorDocumentSwitch(second,false,message,sizeof(message)));
 assert(!SceneEditorRenderJobRead("l3-held",true,message,sizeof(message)));
 assert(!strcmp(bytes,json_object_to_json_string(held)));
 assert(SceneEditorDocumentSwitch(scene,false,message,sizeof(message)));
 json_object *again=SceneEditorRenderJobRead("l3-held",true,message,sizeof(message));assert(again && json_object_equal(again,held));json_object_put(again);
 SceneEditorRenderOptions slow={.width=512,.height=512,.temporal_frames=32,.frame=0,.integrator=RAY_TRACING_3D_INTEGRATOR_DISNEY_V2};
 assert(SceneEditorRenderStart("l3-close",&slow,SceneEditorDocumentRevision(),message,sizeof(message)));
 DestroySceneEditor(editor);active_editor=NULL;
 assert(!strcmp(bytes,json_object_to_json_string(held)));
 snprintf(animSettings.runtimeScenePath,sizeof(animSettings.runtimeScenePath),"%s",scene);assert(InitializeSceneEditor(editor));active_editor=editor;
 again=SceneEditorRenderJobRead("l3-held",true,message,sizeof(message));assert(again && json_object_equal(again,held));json_object_put(again);
 failed=SceneEditorRenderJobRead("l3-close",true,message,sizeof(message));assert(failed && !strcmp(json_object_get_string(authoring_t1_member(failed,"state")),"canceled"));json_object_put(failed);
 json_object_put(held);free(bytes);puts("PASS native held render result across replacement, failed publication, scene switch and pending shutdown/reopen");
}
