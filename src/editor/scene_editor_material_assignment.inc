/* One assignment and navigation shell. Source commands remain retained document
 * transactions; this file only owns selection, confirmation and presentation. */
#include "editor/scene_editor_mesh_preview_render.h"
static SDL_Rect assignment_bounds, assignment_title, assignment_actions[4], assignment_sections[4],
                assignment_resets[2], assignment_preview, assignment_confirm, assignment_cancel,
                assignment_choices[MAX_OBJECTS];
static int assignment_selected = -1, assignment_section = 1, assignment_mode,
           assignment_choice = -1, assignment_scroll, assignment_sources[MAX_OBJECTS];
static bool assignment_expanded = true;
static unsigned long long assignment_revision;
static const char *assignment_presets[] = {"noise3d", "triplanar_checker", "solid"};
static const char *assignment_preset_labels[] = {"Noise", "Triplanar checker", "Solid color"};
static const char *assignment_section_names[] = {"Appearance", "Sources", "Coordinates", "Preview"};
static void assignment_invalidate(void) {
    assignment_bounds=assignment_title=assignment_preview=assignment_confirm=assignment_cancel=(SDL_Rect){0};
    memset(assignment_actions,0,sizeof(assignment_actions));memset(assignment_sections,0,sizeof(assignment_sections));
    memset(assignment_resets,0,sizeof(assignment_resets));memset(assignment_choices,0,sizeof(assignment_choices));
}
static bool assignment_object(int index, SceneEditorDocumentObjectInfo *out) {
    for (int i = 0; i < SceneEditorDocumentObjectCount(); ++i)
        if (SceneEditorDocumentObjectAt(i, out) && out->runtime_index == index) return true;
    return false;
}
static void assignment_close(void) {
    assignment_mode = 0; assignment_choice = -1; assignment_scroll = 0;
    memset(assignment_choices, 0, sizeof(assignment_choices));
    assignment_confirm = assignment_cancel = (SDL_Rect){0};
}
static void assignment_open(int mode) {
    cancel(); graph_panel_reset(); assignment_close(); assignment_mode = mode;
    assignment_revision = SceneEditorDocumentRevision();
    if (mode == 4 || mode == 5) assignment_choice = 0;
}
int SceneEditorSurfaceMaterialSection(void) { return assignment_section; }
bool SceneEditorSurfaceMaterialHeaderModal(void) { return assignment_mode != 0; }
static int assignment_rows(void) {
    if (assignment_mode != 2) return assignment_mode == 1 || assignment_mode == 3 ? 3 : 0;
    int count = 0;
    SceneEditorDocumentObjectInfo item;
    for (int i = 0; i < SceneEditorDocumentObjectCount() && count < MAX_OBJECTS; ++i)
        if (SceneEditorDocumentObjectAt(i, &item) && item.runtime_index >= 0 &&
            item.runtime_index != assignment_selected)
            assignment_sources[count++] = item.runtime_index;
    return count;
}
static void assignment_bound_rect(SDL_Rect* rect, SDL_Rect bounds) {
    if(rect->y<bounds.y || rect->y+rect->h>bounds.y+bounds.h || rect->x<bounds.x || rect->x+rect->w>bounds.x+bounds.w)*rect=(SDL_Rect){0};
}
static int assignment_limit(int y,SDL_Rect b) {
    assignment_bound_rect(&assignment_title,b);assignment_bound_rect(&assignment_preview,b);
    assignment_bound_rect(&assignment_confirm,b);assignment_bound_rect(&assignment_cancel,b);
    for(int i=0;i<4;++i){assignment_bound_rect(&assignment_actions[i],b);assignment_bound_rect(&assignment_sections[i],b);}
    for(int i=0;i<2;++i)assignment_bound_rect(&assignment_resets[i],b);
    for(int i=0;i<MAX_OBJECTS;++i)assignment_bound_rect(&assignment_choices[i],b);
    return y;
}
int SceneEditorSurfaceMaterialHeaderRender(SDL_Renderer *r, SDL_Rect b, int index) {
    assignment_bounds = b;
    memset(assignment_actions, 0, sizeof(assignment_actions));
    memset(assignment_sections, 0, sizeof(assignment_sections));
    memset(assignment_resets, 0, sizeof(assignment_resets));
    memset(assignment_choices, 0, sizeof(assignment_choices));
    assignment_preview = assignment_confirm = assignment_cancel = (SDL_Rect){0};
    if (assignment_selected != index) {
        cancel();graph_panel_reset();assignment_close(); assignment_selected = index; status[0] = 0;
        assignment_section = RuntimeSurfaceGraphActive(index) || RuntimeSurfaceMappingActive(index) ? 1 : 0;
    }
    if (assignment_mode && assignment_revision != SceneEditorDocumentRevision()) {
        assignment_close(); snprintf(status, sizeof(status), "Document changed; choose the assignment again.");
    }
    int y = b.y;
    char text[256];
    SceneEditorDocumentObjectInfo info;
    bool selected_object = assignment_object(index, &info);
    snprintf(text, sizeof(text), "Assignment %s  %.60s", assignment_expanded ? "-" : "+",
             selected_object ? info.name : "Select an object");
    assignment_title = (SDL_Rect){b.x, y, b.w, 26}; draw(r, assignment_title, text, false); y += 29;
    if (!selected_object) return assignment_limit(y,b);
    if (assignment_mode) {
        const char *description = assignment_mode == 1 ? "New source: choose a procedural preset" :
            assignment_mode == 2 ? "Assign an independent copy from an object" :
            assignment_mode == 3 ? "Replace source; retain previous declaration" :
            assignment_mode == 4 ? "Reset source values; keep coordinates" : "Reset coordinates; keep source values";
        SceneEditorLabelLeft(r, (SDL_Rect){b.x,y,b.w,23}, description, (SDL_Color){225,198,153,255}); y += 26;
        assignment_confirm=(SDL_Rect){b.x,y,b.w/2-3,26};
        assignment_cancel=(SDL_Rect){b.x+b.w/2,y,b.w/2,26};
        draw(r,assignment_confirm,assignment_choice>=0?"Confirm":"Choose first",assignment_choice>=0);
        draw(r,assignment_cancel,"Cancel",false); y+=30;
        int count=assignment_rows();
        if(assignment_scroll>=count) assignment_scroll=count?count-1:0;
        for(int i=assignment_scroll;i<count && y+27<=b.y+b.h-28;++i) {
            if(assignment_mode==2) {
                SceneEditorDocumentObjectInfo source;
                assignment_object(assignment_sources[i],&source);
                snprintf(text,sizeof(text),"%.70s",source.name);
            } else snprintf(text,sizeof(text),"%s",assignment_preset_labels[i]);
            assignment_choices[i]=(SDL_Rect){b.x,y,b.w,26};
            draw(r,assignment_choices[i],text,assignment_choice==i); y+=29;
        }
        if(status[0] && y<b.y+b.h) SceneEditorLabelWrapped(r,(SDL_Rect){b.x,y,b.w,b.y+b.h-y},status,(SDL_Color){235,188,149,255});
        return assignment_limit(b.y+b.h,b);
    }
    if (assignment_expanded) {
        json_object *row=current(index);
        const char *family=RuntimeSurfaceGraphActive(index)?"Procedural graph":
            RuntimeSurfaceMappingActive(index)?"Mapped material":"Legacy material";
        json_object *meta=get(row,"material_authoring");
        const char *name=json_object_get_string(get(meta,"name"));
        snprintf(text,sizeof(text),"%s%s%.70s",family,name?": ":"",name?name:"");
        SceneEditorLabelLeft(r,(SDL_Rect){b.x,y,b.w,23},text,(SDL_Color){183,201,214,255}); y+=25;
        if(row)json_object_put(row);
        const char *labels[]={"New", "Assign", "Duplicate", "Replace"};
        for(int i=0;i<4;++i) {
            assignment_actions[i]=(SDL_Rect){b.x+(i%2)*(b.w/2),y,b.w/2-3,25};
            draw(r,assignment_actions[i],labels[i],false);if(i%2)y+=28;
        }
        assignment_resets[0]=(SDL_Rect){b.x,y,b.w/2-3,25};
        assignment_resets[1]=(SDL_Rect){b.x+b.w/2,y,b.w/2-3,25};
        draw(r,assignment_resets[0],"Reset source",false);
        draw(r,assignment_resets[1],"Reset mapping",false); y+=28;
    }
    snprintf(text,sizeof(text),"Preview: %s  >",SceneEditorMeshDisplayModeName(SceneEditorMeshPreviewModeGet()));
    assignment_preview=(SDL_Rect){b.x,y,b.w,25};draw(r,assignment_preview,text,false);y+=28;
    for(int i=0;i<4;++i) {
        assignment_sections[i]=(SDL_Rect){b.x+(i%2)*(b.w/2),y,b.w/2-3,25};
        draw(r,assignment_sections[i],assignment_section_names[i],assignment_section==i);if(i%2)y+=28;
    }
    if(status[0]) {int available=b.y+b.h-y; if(available>0){int h=available<48?available:48;
        int used=SceneEditorLabelWrapped(r,(SDL_Rect){b.x,y,b.w,h},status,(SDL_Color){225,196,153,255});y+=(used<h?used:h)+4;}}
    return assignment_limit(y+4,b);
}
bool SceneEditorSurfaceMaterialHeaderEvent(const SDL_Event *e,int index) {
    if(!e || index!=assignment_selected)return false;
    if(assignment_mode && e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_ESCAPE){assignment_close();return true;}
    if(assignment_mode && e->type==SDL_MOUSEWHEEL) {
        int x,y;SDL_GetMouseState(&x,&y);
        if(!inside(assignment_bounds,x,y))return false;
        assignment_scroll-=e->wheel.y; if(assignment_scroll<0)assignment_scroll=0;
        return true;
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT)return assignment_mode!=0 && (e->type==SDL_KEYDOWN || e->type==SDL_TEXTINPUT);
    int x=e->button.x,y=e->button.y;
    if(assignment_mode) {
        if(inside(assignment_cancel,x,y)){assignment_close();return true;}
        for(int i=0;i<MAX_OBJECTS;++i)if(inside(assignment_choices[i],x,y)){assignment_choice=i;return true;}
        if(inside(assignment_confirm,x,y) && assignment_choice>=0) {
            if(RayTracingDeepRenderDesktopHost_HasActiveWork())return true;
            bool ok=false;
            if(assignment_mode==1 || assignment_mode==3)
                ok=SceneEditorDocumentMaterialPreset(index,assignment_presets[assignment_choice],assignment_mode==3,assignment_revision,status,sizeof(status));
            else if(assignment_mode==2)
                ok=SceneEditorDocumentMaterialAssign(index,assignment_sources[assignment_choice],assignment_revision,status,sizeof(status));
            else if(assignment_mode==4)
                ok=SceneEditorDocumentMaterialResetSource(index,assignment_revision,status,sizeof(status));
            else if(assignment_mode==5)
                ok=SceneEditorDocumentMaterialResetMapping(index,assignment_revision,status,sizeof(status));
            if(ok){graph_panel_reset();assignment_close();assignment_section=1;snprintf(status,sizeof(status),"Material updated; Undo available");}
            return true;
        }
        return inside(assignment_bounds,x,y);
    }
    if(inside(assignment_title,x,y)){assignment_expanded=!assignment_expanded;return true;}
    for(int i=0;i<4;++i)if(inside(assignment_sections[i],x,y)){cancel();graph_panel_reset();assignment_section=i;return true;}
    if(inside(assignment_preview,x,y)) {
        SceneEditorMeshPreviewModeSet(SceneEditorMeshPreviewModeGet()==SCENE_EDITOR_MESH_DISPLAY_MATERIAL?
            SCENE_EDITOR_MESH_DISPLAY_SOLID:SCENE_EDITOR_MESH_DISPLAY_MATERIAL);return true;
    }
    if(RayTracingDeepRenderDesktopHost_HasActiveWork())return false;
    for(int i=0;i<4;++i)if(inside(assignment_actions[i],x,y)) {
        if(i==2) SceneEditorDocumentMaterialDuplicate(index,SceneEditorDocumentRevision(),status,sizeof(status));
        else assignment_open(i==0?1:i==1?2:3);
        return true;
    }
    for(int i=0;i<2;++i)if(inside(assignment_resets[i],x,y)){assignment_open(i+4);return true;}
    return false;
}
static bool assignment_control(const char *name,SDL_Rect *out) {
    const char *actions[]={"new","assign","duplicate","replace"};
    for(int i=0;i<4;++i)if(!strcmp(name,actions[i])){*out=assignment_actions[i];return out->w>0;}
    if(!strcmp(name,"assignment"))*out=assignment_title;
    else if(!strcmp(name,"confirm"))*out=assignment_confirm;
    else if(!strcmp(name,"cancel"))*out=assignment_cancel;
    else if(!strcmp(name,"preview"))*out=assignment_preview;
    else if(!strcmp(name,"source_reset"))*out=assignment_resets[0];
    else if(!strcmp(name,"mapping_reset"))*out=assignment_resets[1];
    else if(!strncmp(name,"section:",8)) {
        for(int i=0;i<4;++i)if(!strcmp(name+8,assignment_section_names[i])){*out=assignment_sections[i];return out->w>0;}
        return false;
    } else if(!strncmp(name,"preset:",7)) {
        for(int i=0;i<3;++i)if(!strcmp(name+7,assignment_presets[i])){*out=assignment_choices[i];return out->w>0;}
        return false;
    } else if(!strncmp(name,"assign:",7)) {
        for(int i=0;i<assignment_rows();++i){SceneEditorDocumentObjectInfo info;
            if(assignment_object(assignment_sources[i],&info)&&!strcmp(name+7,info.id)){*out=assignment_choices[i];return out->w>0;}}
        return false;
    } else return false;
    return out->w>0;
}
