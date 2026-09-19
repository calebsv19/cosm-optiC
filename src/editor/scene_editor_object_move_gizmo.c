#include "editor/scene_editor_object_move_gizmo.h"

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
    return s_drag.active && move_available(s_drag.object_index) &&
        ObjectEditorGetSelectedObjectIndex()==s_drag.object_index &&
        SceneEditorDocumentRevision()==s_drag.revision &&
        strcmp(SceneEditorDocumentPath(),s_drag.document_path)==0 &&
        runtime_scene_bridge_get_last_object_id_for_scene_index(s_drag.object_index,id,sizeof(id)) &&
        strcmp(id,s_drag.object_id)==0;
}

bool SceneEditorObjectMoveGizmoPreviewProjector(int object_index,
    const SceneEditorDigestOverlayProjector* source, SceneEditorDigestOverlayProjector* display) {
    if (!source || !display) return false;
    *display=*source;
    if (!move_transaction_valid() || s_drag.object_index!=object_index) return false;
    /* Translation-only presentation: shift the view, never the live scene or document. */
    display->center_x-=s_drag.preview.position[0]-s_drag.original.position[0];
    display->center_y-=s_drag.preview.position[1]-s_drag.original.position[1];
    display->center_z-=s_drag.preview.position[2]-s_drag.original.position[2];
    return true;
}

static void move_update(int x,int y) {
    double pixels=((double)x-s_drag.start_x)*s_drag.axis_screen_x +
                  ((double)y-s_drag.start_y)*s_drag.axis_screen_y;
    double delta=pixels/s_drag.pixels_per_unit;
    if (!isfinite(delta)) return;
    int component=(int)s_drag.axis-(int)SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    s_drag.preview=s_drag.original;
    s_drag.preview.position[component]+=delta;
}

static bool move_project(const SceneEditorDigestOverlayProjector* projector,
                         const RuntimeSceneBridge3DDigestState* digest,
                         const double position[3],
                         SceneEditorBezier3DGizmoAxis axis,
                         int* ax, int* ay, int* bx, int* by,
                         double* pixels_per_unit) {
    SceneEditorBezier3DInteractionMetrics metrics =
        SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    double ppu = 0.0;
    if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(
        projector, position[0], position[1], position[2], axis,
        metrics.gizmo_world_length, ax, ay, bx, by, &ppu) || ppu <= 0.0) return false;
    /* A scene span can change by orders of magnitude. Keep handles at a stable
       on-screen length so a framed mesh never creates room-sized axes. */
    double screen_length = 72.0;
    double world_length = screen_length / ppu;
    return SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(
        projector, position[0], position[1], position[2], axis,
        world_length, ax, ay, bx, by, pixels_per_unit);
}

static bool move_pick(const SceneEditorDigestOverlayProjector* projector,
                      const RuntimeSceneBridge3DDigestState* digest,
                      const double position[3], int x, int y,
                      SceneEditorBezier3DGizmoAxis* out_axis,
                      double* out_unit_x, double* out_unit_y,
                      double* out_pixels_per_unit) {
    double best = 100.0; /* 10 logical pixels from a handle segment. */
    bool found = false;
    for (int axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z; ++axis) {
        int ax = 0, ay = 0, bx = 0, by = 0;
        double ppu = 0.0;
        if (!move_project(projector, digest, position,
                          (SceneEditorBezier3DGizmoAxis)axis,
                          &ax, &ay, &bx, &by, &ppu) || ppu <= 0.0) continue;
        double dx = (double)bx - ax, dy = (double)by - ay;
        double len2 = dx * dx + dy * dy;
        if (len2 < 64.0) continue;
        /* The square endpoint is the hit target. The shaft is visual only so
           it cannot steal ordinary object-selection clicks behind it. */
        double dist2 = ((double)x - bx) * ((double)x - bx) +
                       ((double)y - by) * ((double)y - by);
        if (dist2 >= best) continue;
        best = dist2;
        found = true;
        *out_axis = (SceneEditorBezier3DGizmoAxis)axis;
        *out_unit_x = dx / sqrt(len2);
        *out_unit_y = dy / sqrt(len2);
        *out_pixels_per_unit = ppu;
    }
    return found;
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
    if (s_drag.active && !move_transaction_valid()) {
        SceneEditorObjectMoveGizmoReset();
        SceneEditorChromeShellSetActionFeedback("Move cancelled: editing context changed",2200);
        return event->type==SDL_MOUSEMOTION || event->type==SDL_MOUSEBUTTONUP;
    }
    if (s_drag.active) {
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
            SceneEditorObjectMoveGizmoReset();
            SceneEditorChromeShellSetActionFeedback("Move cancelled", 1600);
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
            int component = (int)s_drag.axis - (int)SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
            bool moved = fabs(s_drag.preview.position[component] -
                              s_drag.original.position[component]) > 1e-6;
            bool ok = !moved || (selected == s_drag.object_index &&
                SceneEditorDocumentSetTransformForSceneIndex(s_drag.object_index,
                    &s_drag.preview, diagnostics, sizeof(diagnostics)));
            SceneEditorChromeShellSetActionFeedback(ok ? (moved ? "Object moved" : "Move unchanged") :
                                                     diagnostics, 2200);
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
        !move_available(selected) || !SceneEditorGetPaneLayout(&layout) ||
        !SDL_PointInRect(&(SDL_Point){x,y},&layout.viewport_rect) ||
        !SceneEditorDigestOverlayResolve(&digest) ||
        !SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,
            SceneEditorGetViewportNavState(),&projector)) return false;
    SceneEditorDocumentTransform transform={0};
    char diagnostics[256]={0}, object_id[64]={0};
    SceneEditorBezier3DGizmoAxis axis=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    double ux=0.0,uy=0.0,ppu=0.0;
    if (!SceneEditorDocumentGetTransformForSceneIndex(selected,&transform,diagnostics,sizeof(diagnostics)) ||
        !runtime_scene_bridge_get_last_object_id_for_scene_index(selected,object_id,sizeof(object_id)) ||
        !move_pick(&projector,&digest,transform.position,x,y,&axis,&ux,&uy,&ppu)) return false;
    s_hover=axis;
    if (hover) return false;
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
    s_drag.axis_screen_x = ux;
    s_drag.axis_screen_y = uy;
    s_drag.pixels_per_unit = ppu;
    SceneEditorChromeShellSetActionFeedback("Drag axis handle to move; release applies, Escape cancels", 3200);
    return true;
}

void SceneEditorObjectMoveGizmoRender(SDL_Renderer* renderer,
                                     const SceneEditorDigestOverlayProjector* projector,
                                     const RuntimeSceneBridge3DDigestState* digest,
                                     int selected_object_index) {
    SceneEditorDocumentTransform transform = {0};
    char diagnostics[256] = {0};
    static const SDL_Color colors[3] = {
        {235, 105, 105, 255}, {100, 215, 135, 255}, {105, 155, 240, 255}
    };
    if (!renderer || !projector || !digest || !move_available(selected_object_index) ||
        !SceneEditorDocumentGetTransformForSceneIndex(selected_object_index, &transform,
            diagnostics, sizeof(diagnostics))) return;
    if (s_drag.active && !move_transaction_valid()) SceneEditorObjectMoveGizmoReset();
    const double* position = s_drag.active && s_drag.object_index == selected_object_index
        ? s_drag.preview.position : transform.position;
    for (int axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z; ++axis) {
        int ax = 0, ay = 0, bx = 0, by = 0;
        if (!move_project(projector, digest, position,
                (SceneEditorBezier3DGizmoAxis)axis, &ax, &ay, &bx, &by, NULL)) continue;
        bool active=s_drag.active && (int)s_drag.axis==axis;
        bool hovered=!s_drag.active && (int)s_hover==axis;
        SDL_Color color = active ? (SDL_Color){255,220,115,255} :
                          hovered ? (SDL_Color){255,255,255,255} :
                          colors[axis - SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X];
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        SDL_RenderDrawLine(renderer, ax, ay, bx, by);
        SDL_RenderDrawLine(renderer, ax + 1, ay, bx + 1, by);
        SDL_Rect handle = {bx - 5, by - 5, 11, 11};
        SDL_RenderFillRect(renderer, &handle);
        if (active || hovered) {
            SDL_Rect ring={bx-8,by-8,17,17}; SDL_RenderDrawRect(renderer,&ring);
        }
        const char* labels[]={"X","Y","Z"};
        SDL_Rect label={bx+9,by-10,20,20};
        SceneEditorLabel(renderer,label,labels[axis-SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X],color);
    }
    if (s_drag.active && s_drag.object_index == selected_object_index) {
        int ax = 0, ay = 0, bx = 0, by = 0;
        if (SceneEditorDigestOverlayProjectPoint(projector,
                s_drag.original.position[0], s_drag.original.position[1],
                s_drag.original.position[2], &ax, &ay) &&
            SceneEditorDigestOverlayProjectPoint(projector,
                position[0], position[1], position[2], &bx, &by)) {
            SDL_SetRenderDrawColor(renderer, 245, 220, 150, 255);
            SDL_RenderDrawLine(renderer, ax, ay, bx, by);
            SDL_Rect ghost = {bx - 8, by - 8, 16, 16};
            SDL_RenderDrawRect(renderer, &ghost);
        }
    }
}
