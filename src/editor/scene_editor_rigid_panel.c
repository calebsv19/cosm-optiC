#include "scene_editor_rigid_panel.h"
#include "scene_editor_rigid_job.h"
#include "editor/scene_editor_typography.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_rigid.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_shared_text.h"
#include "import/runtime_rigid_result.h"
#include "../platform/ray_tracing_folder_picker.h"
#include <stdio.h>
#include <string.h>
static SDL_Rect controls[5],clip;
static bool details_visible,paint_valid;
static char painted_path[4096];
static unsigned long long painted_revision;
static char rows[16][256],feedback[512],owner_path[4096];
static unsigned long long owner_revision;
static RayTracingFolderPickerRequest picker;
static bool initialized,choosing_profile;
static json_object *get(json_object *o,const char *key) {json_object *v=NULL;if(o)json_object_object_get_ex(o,key,&v);return v;}
static const char *str(json_object *o,const char *key) {json_object *v=get(o,key);return json_object_is_type(v,json_type_string)?json_object_get_string(v):"";}
static bool hit(SDL_Rect r,int x,int y) {return r.w>0 && r.h>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
void SceneEditorRigidPanelClearControls(void) {memset(controls,0,sizeof(controls));paint_valid=false;}
void SceneEditorRigidPanelReset(void) {
    if(initialized) RayTracing_FolderPicker_Cancel(&picker);
    RayTracing_FolderPicker_RequestInit(&picker);initialized=true;feedback[0]=0;details_visible=false;painted_path[0]=0;SceneEditorRigidPanelClearControls();
}
static void poll(void) {
    SceneEditorRigidJobPoll(feedback,sizeof(feedback));
    if(!initialized || !picker.active) return;
    char path[4096];RayTracingFolderPickerResult result=RayTracing_FolderPicker_Poll(&picker,path,sizeof(path));
    if(result==RAY_TRACING_FOLDER_PICKER_PENDING) return;
    if(result==RAY_TRACING_FOLDER_PICKER_CANCELLED) {snprintf(feedback,sizeof(feedback),"Selection canceled.");return;}
    if(result!=RAY_TRACING_FOLDER_PICKER_SELECTED) {snprintf(feedback,sizeof(feedback),"Result picker unavailable or failed.");return;}
    if(strcmp(owner_path,SceneEditorDocumentPath()) || owner_revision!=SceneEditorDocumentRevision()) {snprintf(feedback,sizeof(feedback),"Scene changed during selection; choose the result again.");return;}
    if(choosing_profile) {SceneEditorRigidJobBegin(path,feedback,sizeof(feedback));return;}
    char manifest[4200];snprintf(manifest,sizeof(manifest),"%s/manifest.json",path);
    json_object *m=json_object_from_file(manifest);
    /* Manifest only discovers identity. The retained command independently
     * verifies manifest digest, inventory, physical input and current meshes. */
    SceneEditorTimelinePause();
    SceneEditorDocumentSelectRigidResult(path,str(m,"result_revision"),owner_revision,feedback,sizeof(feedback));
    if(m) json_object_put(m);
}
static bool enabled(unsigned action) {
    if(picker.active) return false;
    if(action==0) return !SceneEditorRigidJobBusy();
    if(action==3) return !SceneEditorRigidJobBusy() && !SceneEditorDocumentIsDirty();
    return true;
}
bool SceneEditorRigidPanelDetailsVisible(void) {return details_visible;}
bool SceneEditorRigidPanelPickerActive(void) {return picker.active;}
const char *SceneEditorRigidPanelFeedback(void) {return feedback;}
bool SceneEditorRigidPanelEvent(SDL_Event *e) {
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT || !paint_valid || !SceneEditorDocumentIsOpen() ||
       strcmp(painted_path,SceneEditorDocumentPath()) || painted_revision!=SceneEditorDocumentRevision() ||
       !hit(clip,e->button.x,e->button.y)) return false;
    for(unsigned i=0;i<5;i++) if(hit(controls[i],e->button.x,e->button.y)) {
        /* The outer router owns press/release, capture and keyboard activation.
         * A stale paint or raw legacy event must never bypass its eligibility. */
        if(!SceneEditorSharedUiActivationIs(212,1+i) || !enabled(i)) return true;
        if(i==4) {details_visible=!details_visible;return true;}
        if(i==0 || i==3) {
            if(!initialized) SceneEditorRigidPanelReset();
            choosing_profile=i==3;
            owner_revision=SceneEditorDocumentRevision();snprintf(owner_path,sizeof(owner_path),"%s",SceneEditorDocumentPath());
            if(!(choosing_profile?RayTracing_FilePicker_Begin(&picker,"Choose a local rigid run profile",owner_path):RayTracing_FolderPicker_Begin(&picker,"Choose a verified local rigid result bundle",owner_path))) snprintf(feedback,sizeof(feedback),"Could not start result picker.");
        } else if(i==1) SceneEditorDocumentValidateRigidResult(feedback,sizeof(feedback));
        else {
            bool ok=SceneEditorDocumentSave(feedback,sizeof(feedback));
            if(ok) snprintf(feedback,sizeof(feedback),"Saved scene and result reference.");
        }
        return true;
    }
    return false;
}
int SceneEditorRigidPanelDraw(SDL_Renderer *r,SDL_Rect pane,int y) {
    poll();clip=pane;
    if(!SceneEditorDocumentIsOpen() || !RuntimeRigidResultActive()) return y;
    if(strcmp(painted_path,SceneEditorDocumentPath())) details_visible=false;
    snprintf(painted_path,sizeof(painted_path),"%s",SceneEditorDocumentPath());
    painted_revision=SceneEditorDocumentRevision();paint_valid=true;
    int x=pane.x+10,w=pane.w-20;
    if(w<80) {paint_valid=false;return y;}
    RayTracingThemePalette p=SceneEditorChromeShellResolvePalette();
    scene_editor_shared_label_at(r,"Rigid result",x,y,p.text_primary,12);
    controls[4]=(SDL_Rect){x+w-76,y-3,76,24};
    SceneEditorSharedUiPanelButton(r,212,5,controls[4],details_visible?"Hide details":"Details",details_visible,enabled(4));y+=28;
    json_object *o=RuntimeRigidResultInspection();
    static RayEvaluatedSceneSnapshot snapshot;
    const RayEvaluatedObjectTransform *moving=NULL;
    if(SceneEditorTimelineCopyEvaluated(&snapshot))
        for(size_t i=0;i<snapshot.object_transform_count;i++)
            if(snapshot.object_transforms[i].source==RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT) {moving=&snapshot.object_transforms[i];}

    json_object *bodies=get(o,"bodies");size_t count=json_object_array_length(bodies);
    snprintf(rows[0],sizeof(rows[0]),"%s | tick %lld / %lld",SceneEditorTimelinePlaying()?"Playing":"Paused",(long long)(moving?moving->source_tick:0),(long long)json_object_get_int64(get(o,"endpoint_tick")));
    SceneEditorLabelWrapped(r,(SDL_Rect){x,y,w,24},rows[0],p.text_primary);y+=24;
    snprintf(rows[1],sizeof(rows[1]),"%zu bodies | %s",count,SceneEditorRigidJobBusy()?"Preparing locally...":"Accepted result");
    SceneEditorLabelWrapped(r,(SDL_Rect){x,y,w,24},rows[1],p.text_primary);y+=24;
    const char *labels[]={"Choose result","Validate","Save scene",SceneEditorRigidJobBusy()?"Preparing...":"Run profile"};
    for(unsigned i=0;i<4;i++) {
        controls[i]=(SDL_Rect){x+(int)(i%2)*((w+6)/2),y+(int)(i/2)*30,(w-6)/2,26};
        SceneEditorSharedUiPanelButton(r,212,1+i,controls[i],labels[i],false,enabled(i));
    }
    y+=64;
    if(feedback[0]) {SceneEditorLabelWrapped(r,(SDL_Rect){x,y,w,60},feedback,p.text_primary);y+=64;}
    if(details_visible) {
        if(moving) {
            snprintf(rows[12],sizeof(rows[12]),"%s COM (m): %.5g, %.5g, %.5g",moving->target_id,moving->position.x,moving->position.y,moving->position.z);
            SceneEditorLabelWrapped(r,(SDL_Rect){x,y,w,40},rows[12],p.text_primary);y+=40;
        }
        const char *names[]={"Result","Physical","Engine"};
        const char *keys[]={"result_revision","physical_input_sha256","engine_sha256"};
        for(unsigned i=0;i<3;i++) {
            snprintf(rows[i],sizeof(rows[i]),"%s: %.20s...",names[i],str(o,keys[i]));
            SceneEditorLabelWrapped(r,(SDL_Rect){x,y,w,36},rows[i],p.text_primary);y+=36;
        }
        for(size_t i=0;i<count && i<8;i++) {
            json_object *b=json_object_array_get_idx(bodies,i);
            snprintf(rows[4+i],sizeof(rows[4+i]),"%s: %s / %s%s",str(b,"object_id"),str(b,"role"),str(b,"motion_owner"),json_object_get_boolean(get(b,"visible"))?"":" (hidden)");
            SceneEditorLabelWrapped(r,(SDL_Rect){x,y,w,36},rows[4+i],p.text_primary);y+=36;
        }
        if(count>8) {scene_editor_shared_label_at(r,"First 8 bodies shown",x,y,p.text_muted,12);y+=22;}
    }
    if(o) json_object_put(o);
    return y+8;
}
bool SceneEditorRigidPanelControl(const char *name,SDL_Rect *out) {
    const char *names[]={"rigid_choose","rigid_validate","rigid_save","rigid_run","rigid_details"};
    if(!name || !out) return false;
    for(int i=0;i<5;i++) if(!strcmp(name,names[i])) {*out=controls[i];return out->w>0;}
    return false;
}
