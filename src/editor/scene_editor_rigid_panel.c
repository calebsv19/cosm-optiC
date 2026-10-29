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
static SDL_Rect controls[4],clip;
static char rows[16][256],feedback[512],owner_path[4096];
static unsigned long long owner_revision;
static RayTracingFolderPickerRequest picker;
static bool initialized,choosing_profile;
static json_object *get(json_object *o,const char *key) {json_object *v=NULL;if(o)json_object_object_get_ex(o,key,&v);return v;}
static const char *str(json_object *o,const char *key) {json_object *v=get(o,key);return json_object_is_type(v,json_type_string)?json_object_get_string(v):"";}
static bool hit(SDL_Rect r,int x,int y) {return r.w>0 && r.h>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
void SceneEditorRigidPanelClearControls(void) {memset(controls,0,sizeof(controls));}
void SceneEditorRigidPanelReset(void) {
    if(initialized) RayTracing_FolderPicker_Cancel(&picker);
    RayTracing_FolderPicker_RequestInit(&picker);initialized=true;feedback[0]=0;SceneEditorRigidPanelClearControls();
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
bool SceneEditorRigidPanelEvent(SDL_Event *e) {
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT || !hit(clip,e->button.x,e->button.y)) return false;
    for(int i=0;i<4;i++) if(hit(controls[i],e->button.x,e->button.y)) {
        if(i==0 || i==3) {
            if(!initialized) SceneEditorRigidPanelReset();
            if(picker.active || SceneEditorRigidJobBusy()) return true;
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
    int x=pane.x+10,w=pane.w-20;
    RayTracingThemePalette p=SceneEditorChromeShellResolvePalette();
    scene_editor_shared_label_at(r,"Rigid simulation result",x,y,p.text_primary,12);y+=25;
    const char *labels[]={"Choose result","Validate","Save scene"};
    for(int i=0;i<3;i++) {controls[i]=(SDL_Rect){x+i*(w+6)/3,y,(w-12)/3,30};SceneEditorSharedUiPanelButton(r,212,1+i,controls[i],labels[i],false,!picker.active);}y+=36;
    controls[3]=(SDL_Rect){x,y,w,30};SceneEditorSharedUiPanelButton(r,212,4,controls[3],SceneEditorRigidJobBusy()?"Preparing locally...":"Run local profile",false,!SceneEditorRigidJobBusy() && !picker.active && !SceneEditorDocumentIsDirty());y+=36;
    json_object *o=RuntimeRigidResultInspection();
    snprintf(rows[0],sizeof(rows[0]),"Result: %.20s...",str(o,"result_revision"));
    snprintf(rows[1],sizeof(rows[1]),"Physical: %.20s...",str(o,"physical_input_sha256"));
    snprintf(rows[2],sizeof(rows[2]),"Engine: %.20s...",str(o,"engine_sha256"));
    TimelineSample sample={0};SceneEditorTimelineCurrentSample(&sample);
    static RayEvaluatedSceneSnapshot snapshot;
    bool evaluated=SceneEditorTimelineCopyEvaluated(&snapshot);
    unsigned tick=0;
    const RayEvaluatedObjectTransform *moving=NULL;
    for(size_t i=0;evaluated && i<snapshot.object_transform_count;i++)
        if(snapshot.object_transforms[i].source==RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT) {
            tick=snapshot.object_transforms[i].source_tick;moving=&snapshot.object_transforms[i];
        }
    snprintf(rows[3],sizeof(rows[3]),"Tick %u / %lld | 240 Hz | %s",tick,(long long)json_object_get_int64(get(o,"endpoint_tick")),SceneEditorTimelinePlaying()?"playing":"paused");
    json_object *bodies=get(o,"bodies");size_t count=json_object_array_length(bodies);
    for(size_t i=0;i<count && i<8;i++) {
        json_object *b=json_object_array_get_idx(bodies,i);
        snprintf(rows[4+i],sizeof(rows[4+i]),"%s: %s / %s%s",str(b,"object_id"),str(b,"role"),str(b,"motion_owner"),json_object_get_boolean(get(b,"visible"))?"":" (hidden)");
    }
    for(size_t i=0;i<4+count && i<12;i++) {scene_editor_shared_label_at(r,rows[i],x,y,p.text_primary,12);y+=21;}
    if(moving) {
        snprintf(rows[12],sizeof(rows[12]),"%s COM (m): %.5g, %.5g, %.5g",moving->target_id,moving->position.x,moving->position.y,moving->position.z);
        scene_editor_shared_label_at(r,rows[12],x,y,p.text_primary,12);y+=22;
    }
    scene_editor_shared_label_at(r,"Play, pause, step and scrub in the timeline.",x,y,p.text_primary,12);y+=24;
    if(feedback[0]) {SceneEditorLabelWrapped(r,(SDL_Rect){x,y,w,66},feedback,p.text_primary);y+=70;}
    if(o) json_object_put(o);
    return y+10;
}
bool SceneEditorRigidPanelControl(const char *name,SDL_Rect *out) {
    const char *names[]={"rigid_choose","rigid_validate","rigid_save","rigid_run"};
    if(!name || !out) return false;
    for(int i=0;i<4;i++) if(!strcmp(name,names[i])) {*out=controls[i];return out->w>0;}
    return false;
}
