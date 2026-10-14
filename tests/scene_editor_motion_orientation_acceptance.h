#include "editor/scene_editor_follower_orientation.h"
#include "editor/scene_editor_motion_plan.h"
#include "editor/scene_editor_motion_plan_panel.h"
static void motion_orientation_run(SceneEditor* editor,const char* scene,const char* mode) {
    char* args[]={"python3","-B",(char*)getenv("OPTIC_MOTION_ORIENTATION_TEST"),(char*)scene,(char*)mode,NULL};assert(args[2]);pid_t pid;
    assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);int status=0;bool finished=false;
    for(int i=0;i<6000;++i) {SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid){finished=true;break;}SDL_Delay(10);}
    if(!finished){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(finished && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
static void motion_orientation_oracle(bool reopen) {
    AnimationConfig original=animSettings;animSettings.interactiveMode=false;animSettings.deepRenderMode=true;
    static RayEvaluatedSceneServiceResult evaluated;assert(RayEvaluatedSceneCaptureSample((TimelineSample){24,0,1},&evaluated));
    RuntimeNative3DPreparedFrame* frame=calloc(1,sizeof(*frame));assert(frame);RuntimeNative3DSamplingContext sampling={.sampleSequence=123};
    assert(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(frame,64,40,&evaluated.snapshot,&sampling));
    unsigned char pixels[64*40*4]={0},expected[64*40*4];RuntimeNative3DRenderStats stats;
    for(int integrator=0;integrator<2;++integrator) {
        assert(RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(pixels,integrator?RAY_TRACING_3D_INTEGRATOR_DISNEY_V2:RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,frame,integrator?2:1,NULL,NULL,NULL,NULL,NULL,NULL,&stats));
        const char* path=integrator?"disney-fixed-sampling.rgba":"direct-fixed-sampling.rgba";FILE* f=fopen(path,reopen?"rb":"wb");assert(f);
        if(reopen) {assert(fread(expected,1,sizeof(expected),f)==sizeof(expected));assert(!memcmp(pixels,expected,sizeof(expected)));}
        else assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);
    }
    RuntimeNative3DPreparedFrame_Free(frame);free(frame);animSettings=original;
}
static void motion_orientation_control(SceneEditor *editor,const char *name){fprintf(stderr,"Motion UI control: %s\n",name);dm4_inspector_control(editor,name);}
static void motion_orientation_button(SceneEditor *editor,const char *name){fprintf(stderr,"Motion UI button: %s\n",name);authoring_control(editor,name);}
static void motion_orientation_acceptance(SceneEditor *editor,const char *scene,bool reopen){
 if(reopen){motion_orientation_run(editor,scene,"reopen");motion_orientation_oracle(true);assert(!SceneEditorDocumentIsDirty());return;}
 choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);motion_orientation_button(editor,"paths");dm2_repair_select_row(editor,"path-1");motion_orientation_button(editor,"path_followers");motion_orientation_control(editor,"path_follower_object");motion_orientation_control(editor,"path_object_picker");motion_orientation_control(editor,"path_object/surface");
 motion_orientation_control(editor,"path_follow_direction");motion_orientation_control(editor,"path_rotation_z");animation_mcp_trial_refuse();authoring_text(editor,"15");motion_orientation_control(editor,"path_start_roll");authoring_text(editor,"25");
 MotionPathBinding b;assert(SceneEditorFollowerOrientationRead("object/surface",&b)&&b.follow_direction&&b.rotation_offset[2]==15&&b.start_roll==25);
 motion_orientation_control(editor,"path_follower_plan");assert(SceneEditorMotionPlanPanelOpen());SceneEditorSessionRuntimeRender(editor);motion_orientation_control(editor,"plan_speed");animation_mcp_trial_refuse();authoring_text(editor,"5");animation_mcp_trial_refuse();motion_orientation_control(editor,"plan_preview");motion_orientation_control(editor,"plan_apply");assert(!SceneEditorMotionPlanPanelInteractionActive());
 /* Imported unknown intent metadata must survive reapply and Save. */
 char m[512];assert(document_begin_command(m,sizeof(m)));json_object *plan=MotionPlanFind(MotionPlansAuthor(document_authoring_root()),"object/surface");assert(plan);json_object_object_add(plan,"producer_plan",json_object_new_string("keep"));json_object *waypoints=authoring_t1_member(plan,"waypoints");json_object_object_add(json_object_array_get_idx(waypoints,0),"producer_waypoint",json_object_new_string("keep"));assert(document_finish_command(m,sizeof(m)));SceneEditorMotionPlanPanelReset();
 motion_orientation_run(editor,scene,"edit");assert(SceneEditorMotionPlanPanelOpenTarget("path-1","object/surface"));SceneEditorSessionRuntimeRender(editor);motion_orientation_control(editor,"plan_speed");authoring_text(editor,"6");motion_orientation_control(editor,"plan_apply");assert(SceneEditorDocumentUndo(m,sizeof(m)));SceneEditorMotionPlanPanelReset();
 assert(SceneEditorChromeActionsSaveAuthoring());motion_orientation_oracle(false);SceneEditorSessionRuntimeRender(editor);capture(editor,"motion-orientation-ui.ppm");fprintf(stderr,"Motion plan/orientation native UI and MCP parity PASS\n");
}
