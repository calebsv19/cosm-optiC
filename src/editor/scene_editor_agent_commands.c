#include "editor/scene_editor_agent_mesh.h"
#include "editor/scene_editor_scene_commands.h"
#include "editor/scene_editor_command_guard.h"
#include "editor/scene_editor_agent_control.h"
#include "editor/scene_editor_atmosphere.h"
#include "editor/scene_editor_atmosphere_panel.h"
#include "editor/scene_editor_atmosphere_viewport.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_chrome_actions.h"
#include "editor/scene_editor_transform_panel.h"
#include "editor/scene_editor_lifecycle.h"
#include "editor/scene_editor_agent_material.h"
#include "editor/scene_editor_agent_object.h"
#include "editor/scene_editor_agent_camera.h"
#include "editor/scene_editor_agent_animation.h"
#include "editor/scene_editor_agent_paths.h"
#include "editor/scene_editor_agent_motion.h"
#include "editor/scene_editor_agent_lighting.h"
#include "editor/scene_editor_agent_render.h"
#include "editor/scene_editor_render_control.h"
#include "editor/bezier_editor.h"
#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_camera_inspector.h"
#include "editor/scene_editor_light_authoring.h"
#include "editor/scene_editor_rename.h"
#include "editor/scene_editor_object_move_gizmo.h"
#include "editor/scene_editor_surface_material_panel.h"
#include <stdio.h>
#include <string.h>

static json_object* get(json_object* o,const char* key) {
    json_object* v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;
}
json_object* SceneEditorAgentControlDispatch(json_object* request) {
    json_object* response=json_object_new_object();char message[512]="";
    json_object* method=get(request,"method"),*params=get(request,"params");
    bool ok=false;
    if(!method || !json_object_is_type(method,json_type_string) || !SceneEditorDocumentIsOpen()) {
        snprintf(message,sizeof(message),"No open scene or invalid command.");goto done;
    }
    const char* name=json_object_get_string(method);
    unsigned long long revision=SceneEditorDocumentRevision();
    if(!strcmp(name,"scene.read")) {ok=true;goto done;}
    if(SceneEditorAgentRenderReadOnly(name)) {
        ok=SceneEditorAgentRenderDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(SceneEditorAgentLightingReadOnly(name)) {
        ok=SceneEditorAgentLightingDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(SceneEditorAgentMotionReadOnly(name)) {
        ok=SceneEditorAgentMotionDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(SceneEditorAgentPathsReadOnly(name)) {
        ok=SceneEditorAgentPathsDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(SceneEditorAgentAnimationReadOnly(name)) {
        ok=SceneEditorAgentAnimationDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(SceneEditorAgentCameraReadOnly(name)) {
        ok=SceneEditorAgentCameraDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(SceneEditorAgentMeshReadOnly(name)) {
        ok=SceneEditorAgentMeshDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(SceneEditorAgentObjectReadOnly(name)) {
        ok=SceneEditorAgentObjectDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(SceneEditorAgentMaterialReadOnly(name)) {
        ok=SceneEditorAgentMaterialDispatch(name,params,response,message,sizeof(message));goto done;
    }
    if(!strcmp(name,"render.cancel")) {
        json_object* expected=get(params,"expected_revision");
        if(expected && json_object_is_type(expected,json_type_int) && json_object_get_int64(expected)>=0 && (unsigned long long)json_object_get_int64(expected)==revision)
            ok=SceneEditorAgentRenderDispatch(name,params,response,message,sizeof(message));
        else snprintf(message,sizeof(message),"Scene changed; supply the current expected_revision.");goto done;
    }
    json_object* owner=get(request,"scene_path");
    if(owner && (!json_object_is_type(owner,json_type_string) || strcmp(json_object_get_string(owner),SceneEditorDocumentPath()))) {snprintf(message,sizeof(message),"Editor changed scenes; read current state before editing.");goto done;}
    if(SceneEditorCommandInteractionActive()) {
        snprintf(message,sizeof(message),"Finish or cancel the active editor operation before agent edits.");goto done;
    }
    json_object* expected=get(params,"expected_revision");
    if(!expected || !json_object_is_type(expected,json_type_int) ||
       json_object_get_int64(expected)<0 || (unsigned long long)json_object_get_int64(expected)!=revision) {
        snprintf(message,sizeof(message),"Scene changed; read current state and supply expected_revision.");goto done;
    }
    if(!strcmp(name,"scene.open") || !strcmp(name,"scene.create") || !strcmp(name,"scene.save_as")) {
        json_object* path=get(params,"scene_path");
        if(json_object_is_type(path,json_type_string) && strlen(json_object_get_string(path))==(size_t)json_object_get_string_len(path)) ok=SceneEditorSceneCommand(name+6,json_object_get_string(path),revision,message,sizeof(message));
        else snprintf(message,sizeof(message),"Scene lifecycle requires an absolute scene_path.");
    } else if(!strncmp(name,"render.",7)) {
        ok=SceneEditorAgentRenderDispatch(name,params,response,message,sizeof(message));
    } else if(!strncmp(name,"lighting.",9)) {
        ok=SceneEditorAgentLightingDispatch(name,params,response,message,sizeof(message));
    } else if(!strncmp(name,"motion_plan.",12) || !strncmp(name,"follower_orientation.",21)) {
        ok=SceneEditorAgentMotionDispatch(name,params,response,message,sizeof(message));
    } else if(!strncmp(name,"path.",5)) {
        ok=SceneEditorAgentPathsDispatch(name,params,response,message,sizeof(message));
    } else if(!strncmp(name,"animation.",10)) {
        ok=SceneEditorAgentAnimationDispatch(name,params,response,message,sizeof(message));
    } else if(!strncmp(name,"camera.",7)) {
        ok=SceneEditorAgentCameraDispatch(name,params,response,message,sizeof(message));
    } else if(!strncmp(name,"mesh.",5)) {
        ok=SceneEditorAgentMeshDispatch(name,params,response,message,sizeof(message));
    } else if(!strncmp(name,"object.",7)) {
        ok=SceneEditorAgentObjectDispatch(name,params,response,message,sizeof(message));
    } else if(!strncmp(name,"material.",9)) {
        ok=SceneEditorAgentMaterialDispatch(name,params,response,message,sizeof(message));
    } else if(!strcmp(name,"atmosphere.set")) {
        json_object* value=get(params,"attachment");json_object* author=json_object_new_object();
        if(value) json_object_object_add(author,"atmosphere_attachment",json_object_get(value));
        SceneAtmosphereAttachment spec;
        ok=value && SceneAtmosphereAttachmentParse(author,&spec,message,sizeof(message)) &&
            SceneEditorAtmosphereSet(&spec,revision,message,sizeof(message));
        json_object_put(author);
    } else if(!strcmp(name,"atmosphere.remove")) ok=SceneEditorAtmosphereRemove(revision,message,sizeof(message));
    else if(!strcmp(name,"atmosphere.import") || !strcmp(name,"atmosphere.generate")) {
        bool preset=!strcmp(name,"atmosphere.generate");json_object* source=get(params,preset?"preset":"source");
        ok=source && json_object_is_type(source,json_type_string) &&
            SceneEditorAtmosphereImport(json_object_get_string(source),preset,revision,message,sizeof(message));
    } else if(!strcmp(name,"scene.undo")) ok=SceneEditorDocumentUndo(message,sizeof(message));
    else if(!strcmp(name,"scene.redo")) ok=SceneEditorDocumentRedo(message,sizeof(message));
    else if(!strcmp(name,"scene.save")) {
        ok=SceneEditorChromeActionsSaveAuthoring();
        if(!ok) snprintf(message,sizeof(message),"%s",SceneEditorChromeActionsSaveError());
    }
    else if(!strcmp(name,"scene.seek")) {
        json_object* frame=get(params,"frame");
        ok=frame && json_object_is_type(frame,json_type_int) && SceneEditorTimelineSeek(json_object_get_int64(frame));
    } else snprintf(message,sizeof(message),"Unsupported scene command.");
done:
    if(!ok && !message[0]) snprintf(message,sizeof(message),"Command failed validation.");
    json_object_object_add(response,"ok",json_object_new_boolean(ok));
    json_object_object_add(response,"message",json_object_new_string(message));
    json_object_object_add(response,"revision",json_object_new_int64(SceneEditorDocumentRevision()));
    json_object_object_add(response,"dirty",json_object_new_boolean(SceneEditorDocumentIsDirty()));
    json_object_object_add(response,"scene_path",json_object_new_string(SceneEditorDocumentPath()?SceneEditorDocumentPath():""));
    SceneAtmosphereAttachment value;
    if(SceneEditorAtmosphereRead(&value) && value.present)
        json_object_object_add(response,"attachment",SceneAtmosphereAttachmentToJson(&value));
    else json_object_object_add(response,"attachment",NULL);
    TimelineSample sample;
    if(SceneEditorTimelineCurrentSample(&sample)) json_object_object_add(response,"frame",json_object_new_int64(sample.absolute_frame));
    RayEvaluatedSceneSnapshot snapshot;
    if(SceneEditorTimelineCopyEvaluated(&snapshot) && snapshot.simulation.valid) {
        json_object* cache=json_object_new_object();
        json_object_object_add(cache,"id",json_object_new_string(snapshot.simulation.cache_id));
        json_object_object_add(cache,"source_frame_index",json_object_new_int64(snapshot.simulation.source_frame_index));
        json_object_object_add(cache,"content_sha256",json_object_new_string(snapshot.simulation.content_digest));
        json_object_object_add(response,"evaluated_cache",cache);
    } else json_object_object_add(response,"evaluated_cache",NULL);
    const char *feedback_method=json_object_get_string(method);
    if(feedback_method && strcmp(feedback_method,"scene.read") && !SceneEditorAgentMaterialReadOnly(feedback_method) && !SceneEditorAgentObjectReadOnly(feedback_method) && !SceneEditorAgentCameraReadOnly(feedback_method) && !SceneEditorAgentLightingReadOnly(feedback_method) && !SceneEditorAgentRenderReadOnly(feedback_method) && !SceneEditorAgentAnimationReadOnly(feedback_method) && !SceneEditorAgentPathsReadOnly(feedback_method) && !SceneEditorAgentMotionReadOnly(feedback_method))
        SceneEditorChromeShellSetActionFeedback(ok?((!strcmp(feedback_method,"scene.open") || !strcmp(feedback_method,"scene.create") || !strcmp(feedback_method,"scene.save_as"))?message:!strcmp(feedback_method,"motion_plan.preview")?"Movement preview ready; scene unchanged.":!strncmp(feedback_method,"render.",7)?"Render command accepted; scene unchanged.":"Agent scene command applied; shared Undo available."):message,6000);
    return response;
}
