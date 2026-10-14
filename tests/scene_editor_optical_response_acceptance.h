#include "render/runtime_material_payload_3d.h"
#include "editor/scene_editor_optical_response.h"
#include "editor/scene_editor_optical_response_panel.h"
static void optical_run(SceneEditor *editor,const char *scene,const char *mode) {
    char *args[]={"python3","-B",(char*)getenv("OPTIC_OPTICAL_RESPONSE_TEST"),(char*)scene,(char*)mode,NULL};assert(args[2]);pid_t pid;assert(posix_spawnp(&pid,"python3",NULL,NULL,args,environ)==0);
    int status=0;bool done=false;for(int i=0;i<12000;i++){SceneEditorAgentControlPoll();SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor);if(waitpid(pid,&status,WNOHANG)==pid){done=true;break;}SDL_Delay(10);}
    if(!done){kill(pid,SIGTERM);waitpid(pid,&status,0);}assert(done&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
}
static void optical_mcp_acceptance(SceneEditor *editor,const char *scene,bool reopen) {
    ObjectEditorSetSelectedObjectIndex(0);SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_MATERIALS);
    assert(SceneEditorFrameViewport(true));SceneEditorSessionRuntimeRender(editor);char message[512];
    fprintf(stderr,"Optical UI focus=%d graph=%d section=%d\n",MaterialEditorResolveFocusedObjectIndex(),RuntimeSurfaceGraphActive(0),SceneEditorSurfaceMaterialSection());
    if(reopen){optical_run(editor,scene,"reopen");motion_orientation_oracle(true);assert(!SceneEditorDocumentIsDirty());return;}
    json_object *before=SceneEditorOpticalResponseRead(0,message,sizeof(message));assert(before);
    SDL_Rect control;assert(SceneEditorSurfaceMaterialPanelControl("section:Appearance",&control));click(editor,control);SceneEditorSessionRuntimeRender(editor);assert(SceneEditorSurfaceMaterialPanelControl("optical_ior_plus",&control));click(editor,control);SceneEditorSessionRuntimeRender(editor);
    json_object *after=SceneEditorOpticalResponseRead(0,message,sizeof(message));assert(after);
    assert(fabs(json_object_get_double(authoring_t1_member(authoring_t1_member(after,"effective"),"ior"))-1.5)<1e-12);
    assert(json_object_to_file("optical-ui-expected.json",after)==0);
    assert(SceneEditorDocumentUndo(message,sizeof(message)));json_object *restored=SceneEditorOpticalResponseRead(0,message,sizeof(message));assert(json_object_equal(before,restored));json_object_put(restored);
    assert(SceneEditorDocumentRedo(message,sizeof(message)));json_object_put(before);json_object_put(after);
    /* Existing legacy glass controls use the same retained transaction/history. */
    ObjectEditorSetSelectedObjectIndex(1);MaterialEditorSetFocusedObjectIndex(1);json_object *settings=json_tokener_parse("{\"base_preset\":\"glass\"}");assert(SceneEditorOpticalResponseSet(1,settings,SceneEditorDocumentRevision(),message,sizeof(message)));json_object_put(settings);
    unsigned long long revision=SceneEditorDocumentRevision();assert(MaterialEditorApplyResponseValueToFocused(MATERIAL_EDITOR_RESPONSE_FIELD_IOR,1.8));assert(SceneEditorDocumentRevision()==revision+1);
    assert(SceneEditorDocumentUndo(message,sizeof(message)));assert(SceneEditorDocumentUndo(message,sizeof(message)));ObjectEditorSetSelectedObjectIndex(0);MaterialEditorSetFocusedObjectIndex(0);
    optical_run(editor,scene,"edit");
    HitInfo3D hit={0};hit.sceneObjectIndex=0;hit.position=vec3(0,0,0);hit.shadingNormal=vec3(0,1,0);hit.geometricNormal=hit.shadingNormal;
    RuntimeMaterialPayload3D payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
    assert(fabs(payload.opticalIor-2.1)<1e-12 && fabs(payload.transparency-.92)<1e-12 && fabs(payload.absorptionDistance-3.5)<1e-12 && !payload.thinWalled);
    assert(fabs(payload.bsdf.roughness-.15)<1e-12 && payload.hasGlassInterfaceTintOverride && payload.hasGlassAbsorptionColorOverride);
    assert(SceneEditorChromeActionsSaveAuthoring());motion_orientation_oracle(false);SceneEditorSessionRuntimeRender(editor);capture(editor,"optical-response-ui.ppm");
}
