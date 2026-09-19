#include "editor/scene_editor_object_move_gizmo.h"

#include "editor/scene_editor_object_transform_handles.h"
#include "editor/scene_editor_object_transform_preview.h"
#include "editor/scene_editor_workspace_layout.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "editor/scene_editor_lifecycle.h"
#include "editor/scene_editor_transform_panel.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/material_editor_authored_texture_binding.h"
#include "editor/scene_editor_typography.h"

#include "app/ray_tracing_deep_render_desktop_host.h"
#include "editor/object_editor.h"
#include "editor/scene_editor.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_digest_overlay_internal.h"
#include "editor/scene_editor_workspace_profile.h"

typedef struct ObjectMoveDrag {
    bool active;
    SceneEditorObjectTransformMode mode;
    SceneEditorObjectTransformHandle handle;
    bool angular_drag;
    double last_angle, accumulated_angle;
    int object_index;
    SceneEditorBezier3DGizmoAxis axis;
    SceneEditorDocumentTransform original;
    SceneEditorDocumentTransform preview;
    unsigned long long revision;
    char object_id[64];
    char document_path[4096];
    int start_x;
    int start_y;
    double axis_screen_x;
    double axis_screen_y;
    double pixels_per_unit;
} ObjectMoveDrag;

static ObjectMoveDrag s_drag;
static SceneEditorObjectTransformMode s_mode;
SceneEditorObjectTransformMode SceneEditorObjectTransformModeGet(void) { return s_mode; }
void SceneEditorObjectTransformModeSet(SceneEditorObjectTransformMode mode) {
    if (mode<0 || mode>SCENE_EDITOR_OBJECT_TRANSFORM_SCALE) return;
    SceneEditorObjectMoveGizmoReset(); s_mode=mode;
}
static SceneEditorBezier3DGizmoAxis s_hover;

void SceneEditorObjectMoveGizmoReset(void) {
    if (s_drag.active) (void)SDL_CaptureMouse(SDL_FALSE);
    memset(&s_drag, 0, sizeof(s_drag));
    s_hover=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
}
SceneEditorBezier3DGizmoAxis SceneEditorObjectMoveGizmoActiveAxis(void) {
    return s_drag.active ? s_drag.axis : SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
}
SceneEditorBezier3DGizmoAxis SceneEditorObjectMoveGizmoHoverAxis(void) { return s_hover; }


static bool move_available(int selected) {
    return SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_SCENE &&
           animSettings.editorMode == EDITOR_MODE_OBJECT &&
           SceneEditorDocumentIsOpen() && selected >= 0 &&
           !SceneEditorLifecycleClosePending() && !SceneEditorWorkspaceProfileMenuOpen() &&
           !SceneEditorTransformPanelInteractionActive() && !SceneEditorSidebarTextActive() &&
           !MaterialEditorAuthoredTextureBindingPickerActive() &&
           !RayTracingDeepRenderDesktopHost_HasActiveWork();
}

static bool move_transaction_valid(void) {
    char id[64]={0};
    return s_drag.active && ObjectEditorTransformHandlesVisible() && move_available(s_drag.object_index) &&
        ObjectEditorGetSelectedObjectIndex()==s_drag.object_index &&
        SceneEditorDocumentRevision()==s_drag.revision &&
        strcmp(SceneEditorDocumentPath(),s_drag.document_path)==0 &&
        runtime_scene_bridge_get_last_object_id_for_scene_index(s_drag.object_index,id,sizeof(id)) &&
        strcmp(id,s_drag.object_id)==0;
}

bool SceneEditorObjectTransformPreview(int object_index,
    SceneEditorDocumentTransform* original,SceneEditorDocumentTransform* preview) {
    if (!move_transaction_valid() || object_index!=s_drag.object_index) return false;
    if (original) *original=s_drag.original;
    if (preview) *preview=s_drag.preview;
    return true;
}

bool SceneEditorObjectMoveGizmoPreviewProjector(int object_index,
    const SceneEditorDigestOverlayProjector* source, SceneEditorDigestOverlayProjector* display) {
    if (!source || !display) return false;
    *display=*source;
    if (!move_transaction_valid() || s_drag.object_index!=object_index ||
        s_drag.mode!=SCENE_EDITOR_OBJECT_TRANSFORM_MOVE) return false;
    /* Translation-only presentation: shift the view, never the live scene or document. */
    display->center_x-=(s_drag.preview.position[0]-s_drag.original.position[0])*SceneEditorDocumentWorldScale();
    display->center_y-=(s_drag.preview.position[1]-s_drag.original.position[1])*SceneEditorDocumentWorldScale();
    display->center_z-=(s_drag.preview.position[2]-s_drag.original.position[2])*SceneEditorDocumentWorldScale();
    return true;
}

static void move_update(int x,int y) {
    double pixels=((double)x-s_drag.start_x)*s_drag.axis_screen_x +
                  ((double)y-s_drag.start_y)*s_drag.axis_screen_y;
    int component=(int)s_drag.axis-1;
    s_drag.preview=s_drag.original;
    if (s_drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE) {
        double delta=pixels/(s_drag.pixels_per_unit*SceneEditorDocumentWorldScale());
        if (isfinite(delta)) s_drag.preview.position[component]+=delta;
    } else if (s_drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_SCALE) {
        if (fabs(pixels)>1e-9) {
            double value=s_drag.original.scale[component]*exp(fmax(-20.0,fmin(20.0,pixels/96.0)));
            if (isfinite(value)) s_drag.preview.scale[component]=fmax(1e-6,value);
        }
    } else {
        double angle;
        if (s_drag.angular_drag && SceneEditorObjectTransformHandleAngle(&s_drag.handle,x,y,&angle)) {
            double step=remainder(angle-s_drag.last_angle,6.2831853071795864769);
            s_drag.accumulated_angle+=step;s_drag.last_angle=angle;
        } else if (!s_drag.angular_drag) s_drag.accumulated_angle=pixels*0.017453292519943295769;
        s_drag.preview.rotation_degrees[component]+=s_drag.accumulated_angle*57.29577951308232;
    }
}

bool SceneEditorObjectMoveGizmoHandleEvent(const SDL_Event* event, SDL_Window* window) {
    SceneEditorPaneLayout layout;
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    int selected = ObjectEditorGetSelectedObjectIndex();
    if (!event || !window) return false;
    Uint32 event_window = 0;
    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: event_window = event->button.windowID; break;
        case SDL_MOUSEMOTION: event_window = event->motion.windowID; break;
        case SDL_KEYDOWN: case SDL_KEYUP: event_window = event->key.windowID; break;
        case SDL_TEXTINPUT: event_window=event->text.windowID; break;
        case SDL_MOUSEWHEEL: event_window=event->wheel.windowID; break;
        case SDL_DROPFILE: event_window=event->drop.windowID; break;
        case SDL_WINDOWEVENT: event_window = event->window.windowID; break;
        default: break;
    }
    if (event_window && event_window != SDL_GetWindowID(window)) return false;
    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT &&
         (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
          event->window.event == SDL_WINDOWEVENT_CLOSE ||
          event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED))) {
        SceneEditorObjectMoveGizmoReset();
        return false;
    }
    if (event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT &&
        move_available(0) && SceneEditorGetPaneLayout(&layout)) {
        SceneEditorWorkspaceChrome chrome;
        SceneEditorWorkspaceLayoutChrome(&layout,&chrome);
        SDL_Point point={event->button.x,event->button.y};
        for (int i=0;i<3;++i) if (SDL_PointInRect(&point,&chrome.transforms[i])) {
            SceneEditorObjectTransformModeSet((SceneEditorObjectTransformMode)i);
            SceneEditorChromeShellSetActionFeedback("",0);return true;
        }
    }
    if (s_drag.active && !move_transaction_valid()) {
        SceneEditorObjectMoveGizmoReset();
        SceneEditorChromeShellSetActionFeedback("Transform cancelled: editing context changed",2200);
        return event->type==SDL_MOUSEMOTION || event->type==SDL_MOUSEBUTTONUP;
    }
    if (s_drag.active) {
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
            SceneEditorObjectMoveGizmoReset();
            SceneEditorChromeShellSetActionFeedback("Transform cancelled", 1600);
            return true;
        }
        if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP ||
            event->type == SDL_TEXTINPUT) return true;
        if (event->type == SDL_MOUSEMOTION) {
            move_update(event->motion.x,event->motion.y);
            return true;
        }
        if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
            char diagnostics[256] = {0};
            move_update(event->button.x,event->button.y);
            double* before=s_drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE ? s_drag.original.position :
                s_drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE ? s_drag.original.rotation_degrees : s_drag.original.scale;
            double* after=s_drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE ? s_drag.preview.position :
                s_drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE ? s_drag.preview.rotation_degrees : s_drag.preview.scale;
            int component=(int)s_drag.axis-1;
            bool moved=fabs(after[component]-before[component]) >
                (s_drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_SCALE ? 1e-12 : 1e-6);
            bool ok=!moved || SceneEditorDocumentSetTransformForSceneIndex(s_drag.object_index,
                &s_drag.preview,diagnostics,sizeof(diagnostics));
            SceneEditorChromeShellSetActionFeedback(ok ? (moved ? "Transform applied" : "Transform unchanged") : diagnostics,2200);
            SceneEditorObjectMoveGizmoReset();
            return true;
        }
        if (event->type==SDL_DROPFILE) { SDL_free(event->drop.file); return true; }
        return event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEWHEEL;
    }
    bool hover=event->type==SDL_MOUSEMOTION;
    if (!hover && (event->type!=SDL_MOUSEBUTTONDOWN || event->button.button!=SDL_BUTTON_LEFT)) return false;
    s_hover=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    int x=hover ? event->motion.x : event->button.x;
    int y=hover ? event->motion.y : event->button.y;
    if ((SDL_GetModState() & (KMOD_ALT|KMOD_CTRL|KMOD_GUI)) ||
        !ObjectEditorTransformHandlesVisible() || !move_available(selected) || !SceneEditorGetPaneLayout(&layout) ||
        !SDL_PointInRect(&(SDL_Point){x,y},&layout.viewport_rect) ||
        !SceneEditorDigestOverlayResolve(&digest) ||
        !SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,
            SceneEditorGetViewportNavState(),&projector)) return false;
    SceneEditorDocumentTransform transform={0};
    char diagnostics[256]={0}, object_id[64]={0};
    SceneEditorBezier3DGizmoAxis axis=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    SceneEditorObjectTransformHandle handle;
    if (!SceneEditorDocumentGetTransformForSceneIndex(selected,&transform,diagnostics,sizeof(diagnostics)) ||
        !runtime_scene_bridge_get_last_object_id_for_scene_index(selected,object_id,sizeof(object_id))) return false;
    double origin[3];
    SceneEditorObjectTransformHandleOrigin(selected,s_mode,transform.position,origin);
    if (!SceneEditorObjectTransformHandlePick(&projector,&digest,origin,s_mode,x,y,&axis,&handle)) return false;
    s_hover=axis;
    if (hover) return false;
    s_drag.mode=s_mode;
    s_drag.handle=handle;
    s_drag.angular_drag=s_mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE &&
        SceneEditorObjectTransformHandleAngle(&handle,x,y,&s_drag.last_angle);
    s_drag.active = true;
    s_drag.object_index = selected;
    s_drag.revision=SceneEditorDocumentRevision();
    memcpy(s_drag.object_id,object_id,sizeof(object_id));
    snprintf(s_drag.document_path,sizeof(s_drag.document_path),"%s",SceneEditorDocumentPath());
    (void)SDL_CaptureMouse(SDL_TRUE);
    s_drag.axis = axis;
    s_drag.original = s_drag.preview = transform;
    s_drag.start_x = event->button.x;
    s_drag.start_y = event->button.y;
    s_drag.axis_screen_x = s_mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE ? 1.0 : handle.ux;
    s_drag.axis_screen_y = s_mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE ? -1.0 : handle.uy;
    s_drag.pixels_per_unit = handle.pixels_per_unit;
    SceneEditorChromeShellSetActionFeedback("Drag labeled axis; release applies, Escape cancels", 3200);
    return true;
}

void SceneEditorObjectMoveGizmoRender(SDL_Renderer* renderer,
                                     const SceneEditorDigestOverlayProjector* projector,
                                     const RuntimeSceneBridge3DDigestState* digest,
                                     int selected_object_index) {
    SceneEditorDocumentTransform transform = {0};
    char diagnostics[256] = {0};
    if (!renderer || !projector || !digest || !ObjectEditorTransformHandlesVisible() || !move_available(selected_object_index) ||
        !SceneEditorDocumentGetTransformForSceneIndex(selected_object_index, &transform,
            diagnostics, sizeof(diagnostics))) return;
    if (s_drag.active && !move_transaction_valid()) SceneEditorObjectMoveGizmoReset();
    const double* position = s_drag.active && s_drag.object_index == selected_object_index
        ? s_drag.preview.position : transform.position;
    double origin[3];
    SceneEditorObjectTransformHandleOrigin(selected_object_index,s_mode,position,origin);
    SceneEditorObjectTransformHandlesRender(renderer,projector,digest,origin,s_mode,s_hover,
        SceneEditorObjectMoveGizmoActiveAxis());
    if (s_drag.active && s_drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE && s_drag.object_index == selected_object_index) {
        int ax = 0, ay = 0, bx = 0, by = 0;
        const double world_scale = SceneEditorDocumentWorldScale();
        if (SceneEditorDigestOverlayProjectPoint(projector,
                s_drag.original.position[0] * world_scale, s_drag.original.position[1] * world_scale,
                s_drag.original.position[2] * world_scale, &ax, &ay) &&
            SceneEditorDigestOverlayProjectPoint(projector,
                position[0] * world_scale, position[1] * world_scale, position[2] * world_scale, &bx, &by)) {
            SDL_SetRenderDrawColor(renderer, 245, 220, 150, 255);
            SDL_RenderDrawLine(renderer, ax, ay, bx, by);
            SDL_Rect ghost = {bx - 8, by - 8, 16, 16};
            SDL_RenderDrawRect(renderer, &ghost);
        }
    }
}
