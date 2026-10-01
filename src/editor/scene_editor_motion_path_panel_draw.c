#include "scene_editor_motion_orientation_panel.h"
/* Presentation for the reusable path library and contextual inspectors. */
#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_camera_inspector.h"
#include "editor/scene_editor_chrome_actions.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_light_authoring.h"
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_object_commands.h"
#include "editor/scene_editor_pointer_event.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_workspace_profile.h"
#include "kit_ui_sdl.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "scene_editor_motion_path_viewport.h"
#include "scene_editor_camera_path_panel.h"
#include "scene_editor_motion_plan_panel.h"
#include "scene_editor_light_path_panel.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene_editor_motion_path_panel_internal.h"
static void label(SDL_Renderer *r, const char *s, int x, int y) {
  char *b = ui.labels[ui.label_count++ % 128];
  snprintf(b, 200, "%s", s);
  ray_tracing_text_draw_utf8_at(
      r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9), b, x, y,
      SceneEditorChromeShellResolvePalette().text_primary);
}
static void scrollbar(SDL_Renderer *r, SDL_Rect pane, int maximum, int offset) {
  if (maximum <= 0)
    return;
  KitUiSdlScrollbarLayout scroll;
  kit_ui_sdl_scrollbar_layout(&pane, pane.h + maximum, offset, &scroll);
  kit_ui_sdl_draw_scrollbar(r, &scroll, (KitRenderColor){40, 44, 50, 255},
                            (KitRenderColor){130, 140, 155, 255});
}
static void button(SDL_Renderer *r, int index, SDL_Rect rect, const char *s,
                   bool enabled) {
  ui.controls[index] = rect;
  ui.enabled[index] = enabled;
  char *b = ui.labels[ui.label_count++ % 128];
  snprintf(b, 200, "%s", s);
  bool selected = (index == SELECT_TOOL && !ui.placing) ||
                  (index == PLACE_TOOL && ui.placing);
  SceneEditorRenderButton(r, rect, b, selected, enabled);
}
static bool projector(const SceneEditorPaneLayout *l) {
  RuntimeSceneBridge3DDigestState d;
  return SceneEditorDigestOverlayResolve(&d) &&
         SceneEditorDigestOverlayBuildProjector(
             &d, &l->viewport_rect, SceneEditorGetViewportNavState(),
             &ui.projector);
}
bool MotionPathPanelProject(const double v[3], int *x, int *y) {
  double s = SceneEditorDocumentWorldScale();
  return SceneEditorDigestOverlayProjectPoint(&ui.projector, v[0] * s, v[1] * s,
                                              v[2] * s, x, y);
}
static void overlay(SDL_Renderer *r, const MotionPath *p) {
  SDL_Rect prior;
  SDL_bool clipped = SDL_RenderIsClipEnabled(r);
  SDL_RenderGetClipRect(r, &prior);
  SDL_RenderSetClipRect(r, &ui.viewport);
  SDL_SetRenderDrawColor(r, 72, 210, 235, 255);
  int px = 0, py = 0;
  bool previous = false;
  for (int i = 0; i <= 256; ++i) {
    double v[3];
    int x, y;
    MotionPathPointAt(p, (double)i * (p->count - 1) / 256, v);
    bool valid = MotionPathPanelProject(v, &x, &y);
    if (valid && previous) {
      SDL_RenderDrawLine(r, px, py, x, y);
      SDL_RenderDrawLine(r, px + 1, py, x + 1, y);
      SDL_RenderDrawLine(r, px, py + 1, x, y + 1);
    }
    px = x;
    py = y;
    previous = valid;
  }
  for (size_t i = 0; i < p->count; ++i) {
    const MotionPathPoint *point = &p->points[i];
    int x, y;
    if (!MotionPathPanelProject(point->position, &x, &y))
      continue;
    SDL_SetRenderDrawColor(r, i == (size_t)ui.point ? 255 : 72,
                           i == (size_t)ui.point ? 210 : 210,
                           i == (size_t)ui.point ? 80 : 235, 255);
    SDL_Rect box = {x - 5, y - 5, 10, 10};
    SDL_RenderFillRect(r, &box);
    if(ui.hover_point==(int)i && !ui.hover_handle && !ui.dragging) {
      SDL_SetRenderDrawColor(r,255,255,255,255);SDL_Rect halo={x-8,y-8,16,16};SDL_RenderDrawRect(r,&halo);
    }
    snprintf(ui.point_labels[i], sizeof(ui.point_labels[i]), "%zu", i + 1);
    ray_tracing_text_draw_utf8_at(
        r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9),
        ui.point_labels[i], x + 9, y - 14, (SDL_Color){230, 250, 255, 255});
    if (ui.show_followers || i != (size_t)ui.point)
      continue;
    for (int h = 0; h < 2; ++h) {
      double v[3];
      for (int k = 0; k < 3; ++k)
        v[k] =
            point->position[k] + (h ? point->outgoing[k] : point->incoming[k]);
      int hx, hy;
      if (MotionPathPanelProject(v, &hx, &hy)) {
        SDL_Color color=h?(SDL_Color){235,155,90,255}:(SDL_Color){160,180,255,255};
        SDL_SetRenderDrawColor(r,color.r,color.g,color.b,255);
        SDL_RenderDrawLine(r, x, y, hx, hy);
        ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,12,9),
            h?"Out":"In",hx+8,hy+5,color);
        SDL_Rect handle = {hx - 4, hy - 4, 8, 8};
        SDL_RenderFillRect(r, &handle);
        if(ui.hover_point==(int)i && ui.hover_handle==h+1 && !ui.dragging) {
          SDL_SetRenderDrawColor(r,255,255,255,255);SDL_Rect halo={hx-7,hy-7,14,14};SDL_RenderDrawRect(r,&halo);
        }
      }
    }
  }
  if(ui.active && !ui.show_followers && ui.point>=0 && ui.point<(int)p->count)
    MotionPointGizmoDraw(r,&ui.projector,&p->points[ui.point],ui.dragging?ui.gizmo_axis:0,ui.gizmo_controls);
  SDL_RenderSetClipRect(r, clipped ? &prior : NULL);
}
void SceneEditorMotionPathOverlayDraw(SceneEditor *e,
                                      const SceneEditorPaneLayout *l) {
  if (ui.active)
    return;
  TimelineTrack track;
  TimelineRate rate;
  TimelineRange range;
  TimelineSample sample;
  if (!SceneEditorTimelineSelectedTrack(&track, &rate, &range, &sample) ||
      (strcmp(track.property_id, MOTION_PROGRESS_PROPERTY) &&
       strcmp(track.property_id, MOTION_CAMERA_PROGRESS_PROPERTY) && strcmp(track.property_id, MOTION_LIGHT_PROGRESS_PROPERTY)))
    return;
  MotionPaths paths;
  if (!SceneEditorMotionPathsRead(&paths))
    return;
  const char *id = NULL;
  for (size_t i = 0; i < paths.binding_count; ++i)
    if (paths.bindings[i].enabled &&
        ((!strcmp(track.property_id, MOTION_CAMERA_PROGRESS_PROPERTY) || !strcmp(track.property_id, MOTION_LIGHT_PROGRESS_PROPERTY))
             ? !strcmp(paths.bindings[i].target_id, track.target_id)
             : !strcmp(paths.bindings[i].object_id, track.target_id + 7)))
      id = paths.bindings[i].path_id;
  for (size_t i = 0; id && i < paths.count; ++i)
    if (!strcmp(paths.paths[i].id, id)) {
      ui.viewport = l->viewport_rect;
      ui.projected = projector(l);
      if (ui.projected)
        overlay(e->renderer, &paths.paths[i]);
      return;
    }
}
static int point_fields(SDL_Renderer *r,const MotionPath *p,int g,int x,int y,int w) {
  const char *groups[]={"Position","Incoming offset","Outgoing offset"};
      label(r, groups[g], x, y);
      y += 20;
      for (int k = 0; k < 3; ++k) {
        int index = g * 3 + k;
        const double *v = g == 0   ? p->points[ui.point].position
                    : g == 1 ? p->points[ui.point].incoming
                             : p->points[ui.point].outgoing;
        char *b = ui.labels[ui.label_count++ % 128];
        if (ui.editing == index)
          snprintf(b, 200, "%c: %s_", 'X' + k, ui.draft);
        else
          snprintf(b, 200, "%c: %.5g", 'X' + k, v[k]);
        ui.fields[index] = (SDL_Rect){x + k * (w + 6) / 3, y, (w - 12) / 3, 24};
        SceneEditorRenderButton(r, ui.fields[index], b, ui.editing == index,
                                true);
      }
      y += 28;
  return y;
}
void SceneEditorMotionPathPanelDraw(SceneEditor *e,
                                    const SceneEditorPaneLayout *l) {
  if (ui.message_revision != SceneEditorDocumentRevision())
    ui.message[0] = 0;
  if (!ui.active)
    return;
  MotionPaths d;
  if (!SceneEditorMotionPathsRead(&d))
    return;
  MotionPath *p = MotionPathPanelSelected(&d);
  MotionOrientationPanelBeginFrame();
  SceneEditorCameraPathPanelReset();
  SceneEditorLightPathPanelReset();
  ui.label_count = 0;
  memset(ui.object_rows,0,sizeof(ui.object_rows));
  memset(ui.controls, 0, sizeof(ui.controls));
  memset(ui.fields, 0, sizeof(ui.fields));
  memset(ui.gizmo_controls,0,sizeof(ui.gizmo_controls));
  memset(ui.rows, 0, sizeof(ui.rows));
  ui.viewport = l->viewport_rect;
  ui.projected = projector(l);
  if (p && ui.projected)
    overlay(e->renderer, ui.dragging ? &ui.drag : p);
  if (p && ui.projected && (ui.placing || (SDL_GetModState() & KMOD_SHIFT)) &&
      !(SDL_GetModState() & (KMOD_ALT | KMOD_CTRL | KMOD_GUI))) {
    SDL_Rect prior_clip;
    SDL_bool was_clipped = SDL_RenderIsClipEnabled(e->renderer);
    SDL_RenderGetClipRect(e->renderer, &prior_clip);
    SDL_RenderSetClipRect(e->renderer, &l->viewport_rect);
    MotionPathViewportGuide(e->renderer, &ui.projector, p, ui.plane_z);
    SDL_RenderSetClipRect(e->renderer, was_clipped ? &prior_clip : NULL);
  }
  SDL_Renderer *r = e->renderer;
  RayTracingThemePalette palette = SceneEditorChromeShellResolvePalette();
  SDL_Rect pane = l->left_content_rect;
  pane.y += 34;
  pane.h -= 34;
  SDL_Rect prior;
  SDL_bool clipped = SDL_RenderIsClipEnabled(r);
  SDL_RenderGetClipRect(r, &prior);
  SDL_RenderSetClipRect(r, &pane);
  SDL_SetRenderDrawColor(r, palette.panel_fill.r, palette.panel_fill.g,
                         palette.panel_fill.b, 255);
  SDL_RenderFillRect(r, &pane);
  int x = pane.x + 10, y = pane.y + 8 - ui.offset, w = pane.w - 20;
  char text[200];
  label(r, "Paths", x, y + 5);
  button(r, NEW, (SDL_Rect){x + w - 92, y, 92, 26}, "+ New path",
         d.count < MOTION_PATH_CAPACITY);
  y += 28;
  if (ui.library_first >= (int)d.count) ui.library_first = 0;
  for (size_t i = ui.library_first; i < d.count && i < (size_t)ui.library_first + 4; ++i) {
    ui.rows[i] = (SDL_Rect){x, y, w, 22};
    char *b = ui.labels[ui.label_count++ % 128];
    snprintf(b, 200, "%s", d.paths[i].name);
    SceneEditorRenderButton(r, ui.rows[i], b, p == &d.paths[i], true);
    y += 24;
  }
  if (d.count > 4) {
    int half = (w - 6) / 2;
    button(r, LIBRARY_PREV, (SDL_Rect){x,y,half,24}, "< Paths", ui.library_first > 0);
    button(r, LIBRARY_NEXT, (SDL_Rect){x+half+6,y,half,24}, "Paths >", ui.library_first + 4 < (int)d.count);
    y += 30;
  }
  if (p) {
    snprintf(text, sizeof(text), ui.editing == 9 ? "Name: %s_" : "Rename: %s",
             ui.editing == 9 ? ui.draft : p->name);
    button(r, NAME, (SDL_Rect){x,y,w-34,24}, text, true);
    button(r, PATH_ACTIONS, (SDL_Rect){x+w-28,y,28,24}, "...", true);
    y += 28;
    int followers = 0;
    for (size_t i=0;i<d.binding_count;++i)
      if (d.bindings[i].enabled && !strcmp(d.bindings[i].path_id,p->id)) ++followers;
    snprintf(text,sizeof(text),"%zu points | %d follower%s",p->count,followers,followers==1?"":"s");
    label(r,text,x,y); y += 20;
    if (ui.show_actions) {
      button(r, DELETE_PATH, (SDL_Rect){x,y,w,22}, "Delete path", followers == 0);
      y += 28;
      if (followers) { label(r,"Detach followers to delete this path.",x,y); y += 20; }
    }
    int third = (w - 12) / 3;
    button(r, SELECT_TOOL, (SDL_Rect){x,y,third,24}, "Move", true);
    button(r, PLACE_TOOL, (SDL_Rect){x+third+6,y,third,24}, "Add points", p->count < MOTION_POINT_CAPACITY);
    button(r, FRAME_PATH, (SDL_Rect){x+2*(third+6),y,third,24}, "Frame", true);
    y += 28;
    if(ui.placing) {label(r,"Shift-click to add | Esc to finish",x,y);y+=20;}
  }
  button(r, ATTACH_SECTION, (SDL_Rect){x,y,w,24}, "Attach / inspect followers >", true);
  y += 28;
  memset(ui.follower_rows,0,sizeof(ui.follower_rows));
  if (p) for (size_t i=0;i<d.binding_count;++i) {
    const MotionPathBinding *binding=&d.bindings[i];
    if (!binding->enabled || strcmp(binding->path_id,p->id)) continue;
    SceneEditorDocumentObjectInfo info;
    const char *name=binding->target_id;
    if (!name[0]) name=SceneEditorDocumentObjectById(binding->object_id,&info)?info.name:binding->object_id;
    char *b=ui.labels[ui.label_count++ % 128];
    snprintf(b,200,"%s >",name);
    ui.follower_rows[i]=(SDL_Rect){x,y,w,22};
    SceneEditorRenderButton(r,ui.follower_rows[i],b,false,true); y+=24;
    if (ui.follower_rows[i].y < pane.y || ui.follower_rows[i].y+ui.follower_rows[i].h > pane.y+pane.h) ui.follower_rows[i]=(SDL_Rect){0};
  }
  if(ui.show_actions) {
    button(r,SAVE,(SDL_Rect){x,y,w,24},"Save scene + animation",true);y+=28;
    button(r,FRAME,(SDL_Rect){x,y,w,24},"Frame scene",true);y+=28;
  }
  ui.max_offset = y + ui.offset - (pane.y + pane.h);
  if (ui.max_offset < 0)
    ui.max_offset = 0;
  for (int i = 0; i < CONTROL_COUNT; ++i)
    if (i != DEPTH && (i == NAME || i < NAME || i > MODE) && (ui.controls[i].y < pane.y ||
        ui.controls[i].y + ui.controls[i].h > pane.y + pane.h))
      ui.controls[i] = (SDL_Rect){0};
  for (size_t i = 0; i < d.count; ++i)
    if (ui.rows[i].y < pane.y || ui.rows[i].y + ui.rows[i].h > pane.y + pane.h)
      ui.rows[i] = (SDL_Rect){0};
  if (ui.controls[ATTACH_SECTION].y < pane.y ||
      ui.controls[ATTACH_SECTION].y + ui.controls[ATTACH_SECTION].h >
          pane.y + pane.h)
    ui.controls[ATTACH_SECTION] = (SDL_Rect){0};
  scrollbar(r, pane, ui.max_offset, ui.offset);
  pane = l->right_content_rect;
  SDL_RenderSetClipRect(r, &pane);
  SDL_SetRenderDrawColor(r, palette.panel_fill.r, palette.panel_fill.g,
                         palette.panel_fill.b, 255);
  SDL_RenderFillRect(r, &pane);
  x = pane.x + 10;
  y = pane.y + 10 - ui.right_offset;
  w = pane.w - 20;
  if (ui.show_followers) {
    button(r,FOLLOWER_BACK,(SDL_Rect){x,y,w,26},"< Point / path shape",true);y+=32;
    const char *types[]={"Object","Camera","Light"};
    for(int i=0;i<3;++i) {
      SDL_Rect rect={x+i*(w+6)/3,y,(w-12)/3,26};
      button(r,FOLLOWER_OBJECT+i,rect,types[i],true);
      SceneEditorRenderButton(r,rect,types[i],ui.follower_type==i,true);
    }
    y+=34;
    char target[TIMELINE_ID_CAPACITY];
    bool attached=MotionPathPanelFollowerTarget(&d,p,target,sizeof(target));
    button(r,FOLLOWER_TIMING,(SDL_Rect){x,y,(w-6)/2,28},"Timing",attached);
    button(r,FOLLOWER_PLAN,(SDL_Rect){x+(w+6)/2,y,(w-6)/2,28},"Movement limits",attached);y+=34;
    if (ui.follower_type==1) {
      y=SceneEditorCameraPathPanelDraw(r,&d,p,pane,x,y,w);
      y=MotionOrientationPanelDraw(r,&d,"camera/main",pane,x,y,w);
    }
    else if (ui.follower_type==2) y=SceneEditorLightPathPanelDraw(r,&d,p,pane,x,y,w);
    else if (p) {
      label(r, "Follower object", x, y);
      y += 26;
      SceneEditorDocumentObjectInfo info;
      bool object = SceneEditorDocumentObjectById(ui.object_id, &info);
      if (!object) {
        SceneEditorObjectReadback read;
        SceneEditorObjectInspect(&read);
        if (read.has_selection) {
          snprintf(ui.object_id, sizeof(ui.object_id), "%s", read.selection.id);
          info = read.selection;
          object = true;
        }
      }
      snprintf(text,sizeof(text),"%s v",object?info.name:"Choose object");
      button(r,OBJECT_PICKER,(SDL_Rect){x,y,w,28},text,true);y+=34;
      button(r,OBJECT_SELECTED,(SDL_Rect){x,y,w,28},"Attach selected scene object",true);y+=34;
      if (ui.object_picker) {
        int count=SceneEditorDocumentObjectCount(),first=ui.object_page;
        if(first>=count) first=ui.object_page=0;
        for (int j=0;j<6 && first+j<count;++j) {
          SceneEditorDocumentObjectInfo candidate;
          if (!SceneEditorDocumentObjectAt(first+j,&candidate)) continue;
          snprintf(ui.picker_ids[j],sizeof(ui.picker_ids[j]),"%s",candidate.id);
          char *b=ui.labels[ui.label_count++ % 128];snprintf(b,200,"%s",candidate.name);
          ui.object_rows[j]=(SDL_Rect){x,y,w,26};
          SceneEditorRenderButton(r,ui.object_rows[j],b,!strcmp(candidate.id,ui.object_id),true);y+=30;
        }
        if(count>6) {
          button(r,OBJECT_PREV,(SDL_Rect){x,y,(w-6)/2,24},"< Objects",first>0);
          button(r,OBJECT_NEXT,(SDL_Rect){x+(w+6)/2,y,(w-6)/2,24},"Objects >",first+6<count);y+=30;
        }
      }
      bool bound = false, restore_known = true;
      int followers = 0;
      for (size_t i = 0; i < d.binding_count; ++i)
        if (d.bindings[i].enabled && !strcmp(d.bindings[i].path_id, p->id)) {
          ++followers;
          if (!d.bindings[i].target_id[0] &&
              !strcmp(d.bindings[i].object_id, ui.object_id)) {
            bound = true;
            restore_known = d.bindings[i].restore_known;
          }
        }
      snprintf(text, sizeof(text), "%d follower%s | world-space route",
               followers, followers == 1 ? "" : "s");
      label(r, text, x, y);
      y += 26;
      button(r, ATTACH, (SDL_Rect){x, y, w, 30},
             bound ? "Attached: on this path" : "Attach on path (replaces XYZ)",
             object && !bound);
      y += 36;
      if(bound) for(size_t j=0;j<d.binding_count;++j) {
        MotionPathBinding *b=&d.bindings[j];
        if(b->target_id[0] || strcmp(b->object_id,ui.object_id))continue;
        button(r,FOLLOW_DIRECTION,(SDL_Rect){x,y,w,28},b->follow_direction?"Follow path direction: On":"Follow path direction: Off",true);y+=34;
        if(b->follow_direction) {
          const char *axes[]={"+X","-X","+Y","-Y","+Z","-Z"};
          snprintf(text,sizeof(text),"Model forward: %s",axes[b->forward_axis]);
          button(r,FORWARD_AXIS,(SDL_Rect){x,y,w,28},text,true);y+=34;
          label(r,"Local rotation offset (degrees)",x,y);y+=24;
          for(int k=0;k<3;++k) {
            if(ui.editing==11+k)snprintf(text,sizeof(text),"%c: %s_",'X'+k,ui.draft);
            else snprintf(text,sizeof(text),"%c: %.3g",'X'+k,b->rotation_offset[k]);
            button(r,ROTATION_X+k,(SDL_Rect){x+k*(w/3),y,w/3-4,28},text,true);
          }
          y+=34;button(r,ROTATION_FROM_BASE,(SDL_Rect){x,y,w,28},"Use base rotation as offset",true);y+=34;
          char orientation_target[TIMELINE_ID_CAPACITY];snprintf(orientation_target,sizeof(orientation_target),"object/%s",b->object_id);
          y=MotionOrientationPanelDraw(r,&d,orientation_target,pane,x,y,w);
          label(r,"Roll rings: drag at route start/end.",x,y);y+=26;
        }
      }
      if (bound && !restore_known) {
        label(r, "Prior source unknown; XYZ stays off.", x, y);
        y += 26;
      }
      button(r, DETACH, (SDL_Rect){x, y, w, 28},
             restore_known ? "Detach: restore prior source" : "Detach to static (legacy)",
             bound);
      y += 34;

      label(r, "Progress 0 = start; 1 = end.", x, y);
      y += 23;
      label(r, "Equal keys pause; later keys resume.", x, y);
      y += 30;

    } else { label(r,"Create a path to attach an object.",x,y); y+=26; }
  } else {
  y = SceneEditorMotionPlanPanelDraw(r, &d, p, pane, x, y, w);
  if (!SceneEditorMotionPlanPanelOpen()) {

  char applied_target[TIMELINE_ID_CAPACITY];
  if(MotionPathPanelAppliedTarget(&d,p,applied_target,sizeof(applied_target))) {
    label(r,"Applied plan locks route edits.",x,y);y+=24;
    button(r,SHAPE_PLAN,(SDL_Rect){x,y,w,24},"Restore / replan follower >",true);y+=34;
  }
  if (p) {
    snprintf(text, sizeof(text), "Point %d / %zu", ui.point + 1, p->count);
    label(r, text, x, y);
    y += 22;
    y=point_fields(r,p,0,x,y,w);
    button(r, PREV, (SDL_Rect){x, y, (w - 6) / 2, 24}, "< Point", true);
    button(r, NEXT, (SDL_Rect){x + (w + 6) / 2, y, (w - 6) / 2, 24}, "Point >",
           true);
    y += 28;
    button(r, ADD, (SDL_Rect){x, y, (w - 6) / 2, 24}, "Split / extend",
           p->count < MOTION_POINT_CAPACITY);
    button(r, REMOVE, (SDL_Rect){x + (w + 6) / 2, y, (w - 6) / 2, 24},
           "Delete point", p->count > 2);
    y += 28;
    button(r, MODE, (SDL_Rect){x, y, w, 24},
           p->points[ui.point].linear ? "Next segment: Straight"
                                      : "Next segment: Cubic Bezier",
           ui.point < (int)p->count - 1);
    y += 28;
    const char *modes[]={"Smooth (L)","Corner","Independent"};
    const int ids[]={HANDLE_MODE,HANDLE_CORNER,HANDLE_INDEPENDENT};
    const MotionHandleMode values[]={MOTION_HANDLE_LINKED,MOTION_HANDLE_CORNER,MOTION_HANDLE_INDEPENDENT};
    for(int k=0;k<3;++k) {
      SDL_Rect rect={x+k*(w+6)/3,y,(w-12)/3,24};
      button(r,ids[k],rect,modes[k],true);
      SceneEditorRenderButton(r,rect,modes[k],p->points[ui.point].handle_mode==values[k],true);
    }
    y += 28;
    button(r,POINT_DETAILS,(SDL_Rect){x,y,w,24},ui.point_details?"Hide point details":"Tangent offsets / draw plane >",true);y+=28;
    if(ui.point_details) {
    char plane[100];
    snprintf(plane, sizeof(plane),
             ui.editing == 10 ? "Draw plane Z: %s_" : "Draw plane Z: %s",
             ui.editing == 10 ? ui.draft : "");
    if (ui.editing != 10)
      snprintf(plane, sizeof(plane), "Draw plane Z: %.5g", ui.plane_z);
    button(r, DEPTH, (SDL_Rect){x, y, w, 24}, plane, true);
    y += 28;
    y=point_fields(r,p,1,x,y,w);
    y=point_fields(r,p,2,x,y,w);
    }
  } else
    label(r, "Choose New Path to begin.", x, y);
  }
  }
  ui.right_max = y + 32 + ui.right_offset - (pane.y + pane.h);
  if (ui.right_max < 0)
    ui.right_max = 0;
  scrollbar(r, pane, ui.right_max, ui.right_offset);
  for (int i = 0; i < 9; ++i)
    if (ui.fields[i].y < pane.y ||
        ui.fields[i].y + ui.fields[i].h > pane.y + pane.h)
      ui.fields[i] = (SDL_Rect){0};
  if (ui.controls[DEPTH].y < pane.y ||
      ui.controls[DEPTH].y + ui.controls[DEPTH].h > pane.y + pane.h)
    ui.controls[DEPTH] = (SDL_Rect){0};
  for (int i=0;i<CONTROL_COUNT;++i)
    if ((i==HANDLE_MODE || i>=FOLLOWER_OBJECT || (i>=OBJECT_PREV && i<=TIMING)) &&
        (ui.controls[i].y<pane.y || ui.controls[i].y+ui.controls[i].h>pane.y+pane.h))
      ui.controls[i]=(SDL_Rect){0};
  for(int i=0;i<6;++i)
    if(ui.object_rows[i].y<pane.y || ui.object_rows[i].y+ui.object_rows[i].h>pane.y+pane.h)
      ui.object_rows[i]=(SDL_Rect){0};
  for (int i = PREV; i <= MODE; ++i)
    if (ui.controls[i].y < pane.y ||
        ui.controls[i].y + ui.controls[i].h > pane.y + pane.h)
      ui.controls[i] = (SDL_Rect){0};
  SDL_RenderSetClipRect(r, clipped ? &prior : NULL);
  SDL_RenderSetClipRect(r,&l->viewport_rect);
  if(ui.show_followers && ui.projected && ui.follower_type!=2)MotionOrientationHandlesDraw(r,&ui.projector,l->viewport_rect);
  SDL_RenderSetClipRect(r,clipped ? &prior : NULL);

}
