#include "scene_editor_motion_orientation_panel.h"
/* Independent path library and geometry authoring. Document commands own
 * history; this module retains only UI selection, text drafts and an
 * uncommitted drag. */
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
#include "motion/scene_motion_plans.h"
#include "scene_editor_light_path_panel.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene_editor_motion_path_panel_internal.h"
MotionPathPanelState motion_path_ui = {.editing = -1};
bool MotionPathPanelAppliedTarget(const MotionPaths *paths,const MotionPath *path,char *out,size_t size) {
  if(!path) return false;
  for(size_t i=0;i<paths->binding_count;++i) {
    const MotionPathBinding *b=&paths->bindings[i];
    if(!b->enabled || strcmp(b->path_id,path->id)) continue;
    char target[TIMELINE_ID_CAPACITY];
    snprintf(target,sizeof(target),b->target_id[0]?"%s":"object/%s",b->target_id[0]?b->target_id:b->object_id);
    if(MotionPlansRuntimeActive(target)) {snprintf(out,size,"%s",target);return true;}
  }
  return false;
}
bool MotionPathPanelFollowerTarget(const MotionPaths *paths,const MotionPath *path,char *out,size_t size) {
  if(!path) return false;
  for(size_t i=0;i<paths->binding_count;++i) {
    const MotionPathBinding *b=&paths->bindings[i];
    if(!b->enabled || strcmp(b->path_id,path->id)) continue;
    bool matches=ui.follower_type==1?!strcmp(b->target_id,"camera/main"):
      ui.follower_type==2?!strncmp(b->target_id,"light/",6):
      (!b->target_id[0] && !strcmp(b->object_id,ui.object_id));
    if(matches) {snprintf(out,size,b->target_id[0]?"%s":"object/%s",b->target_id[0]?b->target_id:b->object_id);return true;}
  }
  return false;
}
static void select_follower_track(const MotionPaths *paths,const MotionPath *path) {
  char target[TIMELINE_ID_CAPACITY];static TimelineDocument doc;
  if(MotionPathPanelFollowerTarget(paths,path,target,sizeof(target)) &&
      SceneEditorDocumentGetTimeline(&doc)==TIMELINE_STATUS_OK) {
    for(size_t i=0;i<doc.track_count;++i) {
      const TimelineTrack *t=&doc.tracks[i];
      if(t->enabled && !strcmp(t->target_id,target) &&
          (!strcmp(t->property_id,MOTION_PROGRESS_PROPERTY) ||
           !strcmp(t->property_id,MOTION_CAMERA_PROGRESS_PROPERTY) ||
           !strcmp(t->property_id,MOTION_LIGHT_PROGRESS_PROPERTY))) {
        SceneEditorTimelineSelectTrack(i);return;
      }
    }
  }
  SceneEditorTimelineClearSelection();
}
static void cancel_field_edit(void) {
  ui.editing = -1;
  ui.draft[0] = 0;
  SDL_StopTextInput();
}
void SceneEditorMotionPathPanelReset(void) {
  MotionOrientationPanelReset();
  SceneEditorMotionPlanPanelReset();
  if (ui.editing >= 0)
    SDL_StopTextInput();
  memset(&ui, 0, sizeof(ui));
  ui.editing = -1;
  ui.hover_point=-1;
}
void SceneEditorMotionPathPanelSelect(bool selected) {
  MotionOrientationPanelReset();
  TimelineTrack prior_track; TimelineRate rate; TimelineRange range; TimelineSample sample;
  bool follower_timing = selected && SceneEditorTimelineSelectedTrack(&prior_track, &rate, &range, &sample) &&
      (!strcmp(prior_track.property_id, MOTION_CAMERA_PROGRESS_PROPERTY) || !strcmp(prior_track.property_id, MOTION_LIGHT_PROGRESS_PROPERTY) || !strcmp(prior_track.property_id,MOTION_PROGRESS_PROPERTY));
  SceneEditorMotionPlanPanelReset();
  ui.active = selected;
  ui.dragging = false;
  ui.editing = -1;
  SDL_StopTextInput();
  SceneEditorTimelinePause();
  if (selected) {
    SceneEditorCameraGestureCancel();
    SceneEditorLightGestureCancel();
    SceneEditorCameraInspectorReset();
    CameraEditorClearSelection();
    BezierEditorSetSelectedPointIndex(-1);
    if(!follower_timing) SceneEditorTimelineClearSelection();
    MotionPaths paths;
    if (!SceneEditorMotionPathsRead(&paths))
      return;
    SceneEditorObjectReadback object;
    SceneEditorObjectInspect(&object);
    if (object.has_selection)
      snprintf(ui.object_id, sizeof(ui.object_id), "%s", object.selection.id);
    for (size_t i = 0; i < paths.binding_count; ++i)
      if (paths.bindings[i].enabled &&
          (follower_timing ? (paths.bindings[i].target_id[0] ? !strcmp(paths.bindings[i].target_id,prior_track.target_id) :
                              (!strncmp(prior_track.target_id,"object/",7) && !strcmp(paths.bindings[i].object_id,prior_track.target_id+7)))
                         : (!paths.bindings[i].target_id[0] && !strcmp(paths.bindings[i].object_id,ui.object_id)))) {
        snprintf(ui.path_id,sizeof(ui.path_id),"%s",paths.bindings[i].path_id);
        if(follower_timing) {
          ui.follower_type=!strncmp(prior_track.target_id,"camera/",7)?1:!strncmp(prior_track.target_id,"light/",6)?2:0;
          ui.show_followers=true;ui.right_offset=0;
        }
      }
    if (!ui.path_id[0] && paths.count)
      snprintf(ui.path_id, sizeof(ui.path_id), "%s", paths.paths[0].id);
  }
}
const char *SceneEditorMotionPathPanelStatus(void) { return ui.message; }
bool SceneEditorMotionPathPanelActive(void) {
  return ui.active &&
         SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_RENDER;
}
static bool hit(SDL_Rect r, int x, int y) {
  return r.w > 0 && r.h > 0 && SDL_PointInRect(&(SDL_Point){x, y}, &r);
}
MotionPath *MotionPathPanelSelected(MotionPaths *d) {
  for (size_t i = 0; i < d->count; ++i)
    if (!strcmp(d->paths[i].id, ui.path_id)) {
      if (ui.point < 0 || ui.point >= (int)d->paths[i].count)
        ui.point = 0;
      return &d->paths[i];
    }
  return NULL;
}
static void pick_point(const MotionPath *p,int x,int y,int *picked,int *kind) {
    double best = 144;
    int point = -1, handle = 0;
    for (size_t i = 0; i < p->count; ++i) {
      for (int h = 0; h < (ui.show_followers?1:3); ++h) {
        if (h && i != (size_t)ui.point)
          continue;
        double v[3];
        for (int k = 0; k < 3; ++k)
          v[k] = p->points[i].position[k] + (h == 1   ? p->points[i].incoming[k]
                                             : h == 2 ? p->points[i].outgoing[k]
                                                      : 0);
        int px, py;
        if (MotionPathPanelProject(v, &px, &py)) {
          double dist = (px - x) * (px - x) + (py - y) * (py - y);
          if (dist < best) {
            best = dist;
            point = i;
            handle = h;
          }
        }
      }
    }
    *picked=point;*kind=handle;
}
static void add_point(MotionPath *p) {
  int at = ui.point;
  MotionPathPoint next = {0};
  unsigned serial = 1;
  bool used;
  do {
    snprintf(next.id, sizeof(next.id), "point-%u", serial++);
    used = false;
    for (size_t i = 0; i < p->count; ++i)
      if (!strcmp(next.id, p->points[i].id))
        used = true;
  } while (used);
  MotionPathPoint *a = &p->points[at];
  next.linear = a->linear;
  if (at == (int)p->count - 1) {
    for (int k = 0; k < 3; ++k) {
      double delta = a->position[k] - p->points[at - 1].position[k];
      next.position[k] = a->position[k] + delta;
      next.incoming[k] = -delta / 3;
      next.outgoing[k] = delta / 3;
    }
  } else {
    MotionPathPoint *b = &p->points[at + 1];
    for (int k = 0; k < 3; ++k) {
      double x = a->position[k], y = x + a->outgoing[k],
             z = b->position[k] + b->incoming[k], w = b->position[k];
      if (a->linear) {
        y = x + (w - x) / 3;
        z = x + 2 * (w - x) / 3;
      }
      double xy = (x + y) / 2, yz = (y + z) / 2, zw = (z + w) / 2,
             left = (xy + yz) / 2, right = (yz + zw) / 2,
             mid = (left + right) / 2;
      next.position[k] = mid;
      next.incoming[k] = left - mid;
      next.outgoing[k] = right - mid;
      if(!a->linear) {a->outgoing[k] = xy - x; b->incoming[k] = zw - w;}
    }
  }
  memmove(&p->points[at + 2], &p->points[at + 1],
          (p->count - at - 1) * sizeof(next));
  p->points[at + 1] = next;
  ++p->count;
  ui.point = at + 1;
}
static bool motion_path_event(SceneEditor *e, SDL_Event *event,
                              const SceneEditorPaneLayout *l) {
  (void)e;
  if (!ui.active)
    return false;
  MotionPaths d;
  if (!SceneEditorMotionPathsRead(&d))
    return false;
  MotionPath *p = MotionPathPanelSelected(&d);
  if(MotionOrientationPanelEvent(event,ui.message,sizeof(ui.message)))return true;
  if(event->type==SDL_MOUSEMOTION && !ui.dragging) {
    ui.hover_point=-1;ui.hover_handle=0;
    SceneEditorObjectTransformHandle handle;
    if(p && ui.projected && hit(l->viewport_rect,event->motion.x,event->motion.y) &&
       !(SDL_GetModState()&(KMOD_ALT|KMOD_CTRL|KMOD_GUI|KMOD_SHIFT)) && !event->motion.state &&
       (ui.show_followers || !MotionPointGizmoPick(&ui.projector,&p->points[ui.point],event->motion.x,event->motion.y,&handle)))
      pick_point(p,event->motion.x,event->motion.y,&ui.hover_point,&ui.hover_handle);
  }
  if(event->type==SDL_WINDOWEVENT && (event->window.event==SDL_WINDOWEVENT_LEAVE || event->window.event==SDL_WINDOWEVENT_FOCUS_LOST))ui.hover_point=-1;
  if(event->type==SDL_MOUSEBUTTONDOWN)
    ui.point_focus=hit(l->viewport_rect,event->button.x,event->button.y) ||
        (!ui.show_followers && !SceneEditorMotionPlanPanelOpen() &&
         hit(l->right_content_rect,event->button.x,event->button.y));
  if(event->type==SDL_WINDOWEVENT && event->window.event==SDL_WINDOWEVENT_FOCUS_LOST)
    ui.point_focus=false;
  /* Navigation owns modified clicks, including clicks on existing handles. */
  if ((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP ||
       event->type == SDL_MOUSEMOTION) &&
      ((SDL_GetModState() & (KMOD_ALT | KMOD_CTRL | KMOD_GUI)) ||
       (event->type == SDL_MOUSEMOTION && (event->motion.state & (SDL_BUTTON_RMASK | SDL_BUTTON_MMASK))) ||
       (event->type != SDL_MOUSEMOTION && event->button.button != SDL_BUTTON_LEFT))) {
    ui.dragging = false;
    return false;
  }
  if (ui.editing >= 0 &&
      (event->type == SDL_TEXTINPUT || event->type == SDL_KEYDOWN)) {
    if (event->type == SDL_TEXTINPUT) {
      if (strlen(ui.draft) + strlen(event->text.text) < sizeof(ui.draft))
        strcat(ui.draft, event->text.text);
      return true;
    }
    if (event->key.keysym.sym == SDLK_ESCAPE) {
      ui.editing = -1;
      SDL_StopTextInput();
      return true;
    }
    if (event->key.keysym.sym == SDLK_BACKSPACE) {
      size_t n = strlen(ui.draft);
      if (n)
        ui.draft[n - 1] = 0;
      return true;
    }
    if (event->key.keysym.sym == SDLK_RETURN && p) {
      if (ui.editing == 10) {
        char *end;
        double v = strtod(ui.draft, &end);
        if (end != ui.draft && !*end && isfinite(v) && fabs(v) <= 1e12) {
          ui.plane_z = v;
          ui.editing = -1;
          SDL_StopTextInput();
        } else
          snprintf(ui.message, sizeof(ui.message),
                   "Enter a finite drawing height.");
        return true;
      }

      if(ui.editing>=11 && ui.editing<=13) {
        char *end;double value=strtod(ui.draft,&end);
        if(end!=ui.draft && !*end && isfinite(value) && fabs(value)<=36000) {
          for(size_t j=0;j<d.binding_count;++j) if(!d.bindings[j].target_id[0] && !strcmp(d.bindings[j].object_id,ui.object_id)) {
            d.bindings[j].rotation_offset[ui.editing-11]=value;
            if(SceneEditorMotionPathsSet(&d,ui.revision,ui.message,sizeof(ui.message)))cancel_field_edit();
            return true;
          }
        }
        snprintf(ui.message,sizeof(ui.message),"Enter rotation degrees between -36000 and 36000.");return true;
      }
      bool valid = true;
      if (ui.editing == 9) {
        valid = *ui.draft;
        snprintf(p->name, sizeof(p->name), "%s", ui.draft);
      } else {
        char *end;
        double value = strtod(ui.draft, &end);
        valid =
            end != ui.draft && !*end && isfinite(value) && fabs(value) <= 1e12;
        double *v = ui.editing < 3   ? p->points[ui.point].position
                    : ui.editing < 6 ? p->points[ui.point].incoming
                                     : p->points[ui.point].outgoing;
        if (valid) {
          double next[3]; memcpy(next,v,sizeof(next));next[ui.editing%3]=value;
          if(ui.editing<3) memcpy(v,next,sizeof(next));
          else valid=MotionPathEditHandle(&p->points[ui.point],ui.editing<6,next);
        }
      }
      if (valid && SceneEditorMotionPathsSet(&d, ui.revision, ui.message,
                                             sizeof(ui.message))) {
        ui.editing = -1;
        SDL_StopTextInput();
        snprintf(ui.message, sizeof(ui.message),
                 "Path updated. Undo is available.");
      } else if (!valid)
        snprintf(ui.message, sizeof(ui.message),
                 "Enter a valid name or finite number.");
      return true;
    }
    return true;
  }
  if (ui.dragging && (ui.revision != SceneEditorDocumentRevision() ||
                      (event->type == SDL_WINDOWEVENT &&
                       (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                        event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED))))
    ui.dragging = false;
  if (ui.dragging) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
      ui.dragging = false;
      return true;
    }
    if (event->type == SDL_MOUSEMOTION && p) {
      if(ui.gizmo_axis) {
        MotionPointGizmoMove(&ui.drag.points[ui.point],ui.gizmo_axis,ui.gizmo_initial,
            &ui.gizmo_handle,event->motion.x-ui.down_x,event->motion.y-ui.down_y);
        return true;
      }
      double x, y, z, s = SceneEditorDocumentWorldScale();
      MotionPathPoint *point = &ui.drag.points[ui.point];
      double plane = point->position[2] + (ui.handle == 1   ? point->incoming[2]
                                           : ui.handle == 2 ? point->outgoing[2]
                                                            : 0);
      if (SceneEditorDigestOverlayScreenRayToPlanePoint(
              &ui.projector, event->motion.x, event->motion.y, plane * s, &x,
              &y, &z)) {
        double *v = ui.handle == 1   ? point->incoming
                    : ui.handle == 2 ? point->outgoing
                                     : point->position;
        double next[3]={x/s-(ui.handle?point->position[0]:0),y/s-(ui.handle?point->position[1]:0),v[2]};
        if(ui.handle) MotionPathEditHandle(point,ui.handle==1,next);
        else memcpy(v,next,sizeof(next));
      }
      return true;
    }
    if (event->type == SDL_MOUSEBUTTONUP) {
      ui.dragging = false;
      if (p && memcmp(p,&ui.drag,sizeof(*p)) &&
          (abs(event->button.x - ui.down_x) > 2 ||
           abs(event->button.y - ui.down_y) > 2)) {
        *p = ui.drag;
        SceneEditorMotionPathsSet(&d, ui.revision, ui.message,
                                  sizeof(ui.message));
      }
      return true;
    }
  }
  if (event->type == SDL_MOUSEWHEEL) {
    int x, y;
    SceneEditorWheelPosition(event, &x, &y);
    if (hit(l->right_content_rect, x, y)) {
      ui.right_offset -= event->wheel.y * 32;
      ui.right_offset = ui.right_offset < 0              ? 0
                        : ui.right_offset > ui.right_max ? ui.right_max
                                                         : ui.right_offset;
      return true;
    }
    if (hit(l->left_content_rect, x, y)) {
      ui.offset -= event->wheel.y * 32;
      ui.offset = ui.offset < 0               ? 0
                  : ui.offset > ui.max_offset ? ui.max_offset
                                              : ui.offset;
      return true;
    }
  }
  if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_l && !event->key.repeat &&
      !(event->key.keysym.mod & (KMOD_CTRL|KMOD_GUI|KMOD_ALT)) &&
      ui.point_focus && !SDL_IsTextInputActive() && !ui.show_followers && !SceneEditorMotionPlanPanelOpen() && p) {
    if(MotionPathSmoothPoint(p,ui.point))
      SceneEditorMotionPathsSet(&d,SceneEditorDocumentRevision(),ui.message,sizeof(ui.message));
    else snprintf(ui.message,sizeof(ui.message),"Move coincident points apart before smoothing.");
    return true;
  }
  if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
    ui.placing = false;
    ui.dragging = false;
    snprintf(ui.message, sizeof(ui.message),
             "Placement finished. Select or drag a point.");
    return true;
  }
  if (event->type != SDL_MOUSEBUTTONDOWN ||
      event->button.button != SDL_BUTTON_LEFT)
    return false;
  int x = event->button.x, y = event->button.y;
  if (SceneEditorCameraPathPanelEvent(event, p, ui.message, sizeof(ui.message)) || SceneEditorLightPathPanelEvent(event, p, ui.message, sizeof(ui.message))) {
    cancel_field_edit(); ui.message_revision = SceneEditorDocumentRevision(); return true;
  }
  for (int j=0;j<6;++j) if(hit(ui.object_rows[j],x,y)) {
    cancel_field_edit();
    snprintf(ui.object_id,sizeof(ui.object_id),"%s",ui.picker_ids[j]);
    ui.object_picker=false; ui.right_offset=0;select_follower_track(&d,p);
    return true;
  }
  for (size_t j=0;j<d.binding_count;++j) if(hit(ui.follower_rows[j],x,y)) {
    cancel_field_edit();SceneEditorMotionPlanPanelReset();
    const MotionPathBinding *binding=&d.bindings[j];
    ui.follower_type=!strncmp(binding->target_id,"camera/",7)?1:binding->target_id[0]?2:0;
    if(!ui.follower_type) snprintf(ui.object_id,sizeof(ui.object_id),"%s",binding->object_id);
    ui.show_followers=true;ui.right_offset=0;ui.object_picker=false;select_follower_track(&d,p);
    return true;
  }
  for (size_t i = 0; i < d.count; ++i)
    if (hit(ui.rows[i], x, y)) {
      cancel_field_edit();
      SceneEditorTimelineClearSelection();SceneEditorMotionPlanPanelReset();
      snprintf(ui.path_id, sizeof(ui.path_id), "%s", d.paths[i].id);
      ui.point = 0;
      ui.show_actions = false;
      ui.show_followers = false;
      ui.right_offset = 0;
      return true;
    }
  for (int i = 0; i < 9; ++i)
    if (p && hit(ui.fields[i], x, y)) {
      ui.editing = i;
      ui.draft[0] = 0;
      ui.revision = SceneEditorDocumentRevision();
      SDL_StartTextInput();
      return true;
    }
  for (int i = 0; i < CONTROL_COUNT; ++i)
    if (hit(ui.controls[i], x, y)) {
      if (!ui.enabled[i])
        return true;
      if(i>=FOLLOW_DIRECTION && i<=ROTATION_FROM_BASE) {
        for(size_t j=0;j<d.binding_count;++j) {
          MotionPathBinding *b=&d.bindings[j];
          if(b->target_id[0] || strcmp(b->object_id,ui.object_id))continue;
          if(i==ROTATION_FROM_BASE) {
            SceneEditorDocumentObjectInfo object;SceneEditorDocumentTransform transform;
            if(SceneEditorDocumentObjectById(ui.object_id,&object) && SceneEditorDocumentGetTransformForSceneIndex(object.runtime_index,&transform,ui.message,sizeof(ui.message))) {
              memcpy(b->rotation_offset,transform.rotation_degrees,sizeof(b->rotation_offset));
              SceneEditorMotionPathsSet(&d,SceneEditorDocumentRevision(),ui.message,sizeof(ui.message));
            }
            cancel_field_edit();return true;
          }
          if(i>=ROTATION_X) {ui.editing=11+i-ROTATION_X;ui.draft[0]=0;ui.revision=SceneEditorDocumentRevision();SDL_StartTextInput();return true;}
          cancel_field_edit();
          if(i==FOLLOW_DIRECTION)b->follow_direction=!b->follow_direction;else b->forward_axis=(b->forward_axis+1)%6;
          SceneEditorMotionPathsSet(&d,SceneEditorDocumentRevision(),ui.message,sizeof(ui.message));return true;
        }
        return true;
      }
      if (i == SELECT_TOOL || i == PLACE_TOOL) {
        ui.editing = -1;
        SDL_StopTextInput();
        ui.placing = i == PLACE_TOOL;
        ui.message[0] = 0;
        return true;
      }
      if (i == FRAME_PATH && p) {
        MotionPathViewportFrame(p, &l->viewport_rect);
        return true;
      }
      if (i == PATH_ACTIONS) {
        cancel_field_edit();
        ui.show_actions = !ui.show_actions;
        return true;
      }
      if (i == LIBRARY_PREV || i == LIBRARY_NEXT) {
        cancel_field_edit();
        ui.library_first += i == LIBRARY_PREV ? -4 : 4;
        return true;
      }
      if(i==FOLLOWER_TIMING) {cancel_field_edit();select_follower_track(&d,p);SceneEditorRenderAuthoringSetTiming(true);return true;}
      if(i==POINT_DETAILS) {cancel_field_edit();ui.point_details=!ui.point_details;return true;}
      if((i==FOLLOWER_PLAN || i==SHAPE_PLAN) && p) {
        cancel_field_edit();char target[TIMELINE_ID_CAPACITY];
        if((i==SHAPE_PLAN?MotionPathPanelAppliedTarget(&d,p,target,sizeof(target)):MotionPathPanelFollowerTarget(&d,p,target,sizeof(target))) && SceneEditorMotionPlanPanelOpenTarget(p->id,target)) {
          ui.show_followers=false;ui.right_offset=0;ui.dragging=false;
        }
        return true;
      }
      if (i == ATTACH_SECTION || i == FOLLOWER_BACK ||
          (i >= FOLLOWER_OBJECT && i <= FOLLOWER_LIGHT)) {
        cancel_field_edit(); SceneEditorMotionPlanPanelReset();
        ui.show_followers=i!=FOLLOWER_BACK;
        if(ui.show_followers) ui.placing=false;
        if(i>=FOLLOWER_OBJECT && i<=FOLLOWER_LIGHT) ui.follower_type=i-FOLLOWER_OBJECT;
        ui.right_offset=0;ui.object_picker=false;
        if(ui.show_followers) select_follower_track(&d,p);
        return true;
      }
      if(i==OBJECT_PICKER) {cancel_field_edit();ui.object_picker=!ui.object_picker;return true;}
      if(i==OBJECT_SELECTED) {
        cancel_field_edit();
        SceneEditorObjectReadback read;SceneEditorObjectInspect(&read);
        if(read.has_selection && p) {
          snprintf(ui.object_id,sizeof(ui.object_id),"%s",read.selection.id);
          if(SceneEditorTimelineCurrentSample(&(TimelineSample){0}) || SceneEditorTimelineActivate())
            SceneEditorMotionPathBind(ui.object_id,p->id,true,SceneEditorDocumentRevision(),ui.message,sizeof(ui.message));
        }
        else snprintf(ui.message,sizeof(ui.message),"Select a scene object first, or use Choose object.");
        ui.object_picker=false;return true;
      }
      if (i == DEPTH) {
        ui.editing = 10;
        ui.draft[0] = 0;
        SDL_StartTextInput();
        return true;
      }
      unsigned long long rev = SceneEditorDocumentRevision();
      bool changed = false;
      if (i == NEW) {
        double origin[3] = {0};
        SceneEditorObjectReadback read;
        SceneEditorObjectInspect(&read);
        SceneEditorDocumentTransform t;
        if (read.has_selection &&
            SceneEditorDocumentGetTransformForSceneIndex(
                read.selection.runtime_index, &t, NULL, 0))
          memcpy(origin, t.position, sizeof(origin));
        double length = ui.projected ? fmax(.01, ui.projector.span_max * .2) /
                                           SceneEditorDocumentWorldScale()
                                     : 2 / SceneEditorDocumentWorldScale();
        if (!read.has_selection && ui.projected) {
          origin[0] = ui.projector.center_x / SceneEditorDocumentWorldScale();
          origin[1] = ui.projector.center_y / SceneEditorDocumentWorldScale();
          origin[2] = ui.projector.center_z / SceneEditorDocumentWorldScale();
        }
        char name[128];
        unsigned number = 1;
        bool used;
        do {
          snprintf(name,sizeof(name),"Path %u",number++);
          used = false;
          for (size_t j=0;j<d.count;++j)
            if (!strcmp(d.paths[j].name,name)) used = true;
        } while (used);
        if (SceneEditorMotionPathCreate(name, origin, length, rev,
                                        ui.path_id, sizeof(ui.path_id),
                                        ui.message, sizeof(ui.message))) {
          ui.point = 0;
          ui.library_first = ((int)d.count / 4) * 4;
          ui.offset = ui.right_offset = 0;
          ui.show_actions = false;
          snprintf(ui.message,sizeof(ui.message),"Created %s. Existing paths are retained.",name);
          ui.placing = true;
          ui.show_followers = false;
          ui.plane_z = origin[2];
          MotionPaths fresh;
          if (SceneEditorMotionPathsRead(&fresh)) {
            MotionPath *created = MotionPathPanelSelected(&fresh);
            if (created)
              MotionPathViewportFrame(created, &l->viewport_rect);
          }
          ui.editing = 9;
          ui.draft[0] = 0;
          ui.revision = SceneEditorDocumentRevision();
          SDL_StartTextInput();
        }
        return true;
      }
      if (i == OBJECT_PREV || i == OBJECT_NEXT) {
        ui.object_page += i == OBJECT_PREV ? -6 : 6;
        ui.right_offset=0;
        return true;
      }
      if (i == SAVE) {
        SceneEditorChromeActionsSaveAuthoring();
        return true;
      }
      if (i == FRAME) {
        SceneEditorFrameViewport(false);
        return true;
      }
      if (!p)
        return true;
      if (i == NAME) {
        ui.editing = 9;
        ui.draft[0] = 0;
        ui.revision = rev;
        SDL_StartTextInput();
        return true;
      }
      if (i == PREV || i == NEXT) {
        cancel_field_edit();
        ui.point = (ui.point + (i == PREV ? -1 : 1) + p->count) % p->count;
        return true;
      }
      if (i == ADD) {
        cancel_field_edit();
        add_point(p);
        changed = true;
      }
      if (i == REMOVE || i == DELETE_TOOL) {
        cancel_field_edit();
        memmove(&p->points[ui.point], &p->points[ui.point + 1],
                (p->count - ui.point - 1) * sizeof(p->points[0]));
        --p->count;
        if (ui.point >= (int)p->count)
          --ui.point;
        changed = true;
      }
      if (i == HANDLE_MODE || i == HANDLE_CORNER || i == HANDLE_INDEPENDENT) {
        cancel_field_edit();
        changed=i==HANDLE_MODE ? MotionPathSmoothPoint(p,ui.point) :
            MotionPathSetHandleMode(&p->points[ui.point],i==HANDLE_CORNER?MOTION_HANDLE_CORNER:MOTION_HANDLE_INDEPENDENT);
        if(!changed) snprintf(ui.message,sizeof(ui.message),"Move coincident points apart before smoothing.");
      }
      if (i == MODE) {
        p->points[ui.point].linear = !p->points[ui.point].linear;
        changed = true;
      }
      if (i == DELETE_PATH) {
        cancel_field_edit();
        for (size_t j = 0; j < d.binding_count; ++j)
          if (!strcmp(d.bindings[j].path_id, p->id) && d.bindings[j].enabled) {
            snprintf(ui.message, sizeof(ui.message),
                     "Detach follower %s before deleting.",
                     d.bindings[j].target_id[0] ? d.bindings[j].target_id : d.bindings[j].object_id);
            return true;
          }
        /* Remove inactive references too; their disabled keys remain retained.
         */
        for (size_t j = 0; j < d.binding_count;)
          if (!strcmp(d.bindings[j].path_id, p->id)) {
            memmove(&d.bindings[j], &d.bindings[j + 1],
                    (--d.binding_count - j) * sizeof(d.bindings[0]));
          } else
            ++j;
        size_t index = p - d.paths;
        memmove(p, p + 1, (--d.count - index) * sizeof(*p));
        changed = true;
        ui.path_id[0] = 0;
      }
      if (i == ATTACH || i == DETACH) {
        if (!SceneEditorTimelineCurrentSample(&(TimelineSample){0}) &&
            !SceneEditorTimelineActivate()) {
          snprintf(ui.message, sizeof(ui.message), "%s",
                   SceneEditorTimelineStatus());
          return true;
        }
        SceneEditorMotionPathBind(ui.object_id, p->id, i == ATTACH,
                                  SceneEditorDocumentRevision(), ui.message,
                                  sizeof(ui.message));
        return true;
      }
      if (i == TIMING) {
        static TimelineDocument doc;
        if (SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK)
          for (size_t j = 0; j < doc.track_count; ++j)
            if (doc.tracks[j].enabled &&
                !strcmp(doc.tracks[j].property_id, MOTION_PROGRESS_PROPERTY) &&
                !strcmp(doc.tracks[j].target_id + 7, ui.object_id)) {
              SceneEditorTimelineSelectTrack(j);
              SceneEditorMotionPathPanelSelect(false);
              SceneEditorRenderAuthoringSetTiming(true);
              break;
            }
        return true;
      }
      if (changed)
        SceneEditorMotionPathsSet(&d, rev, ui.message, sizeof(ui.message));
      return true;
    }
  if (hit(l->viewport_rect, x, y) && p && ui.projected) {
    if (SDL_GetModState() & KMOD_SHIFT) {
      double position[3];
      ui.editing = -1;
      SDL_StopTextInput();
      if (MotionPathViewportPlacement(&ui.projector, x, y, ui.plane_z,
                                      position) &&
          MotionPathViewportAppend(p, position)) {
        if (SceneEditorMotionPathsSet(&d, SceneEditorDocumentRevision(),
                                      ui.message, sizeof(ui.message))) {
          ui.point = (int)p->count - 1;
          snprintf(ui.message, sizeof(ui.message),
                   "Added point %zu. Shift-click to add more.", p->count);
        }
      } else
        snprintf(ui.message, sizeof(ui.message),
                 "Cannot place here: change view/plane or remove a point.");
      return true;
    }

    int axis=ui.show_followers?0:MotionPointGizmoPick(&ui.projector,&p->points[ui.point],x,y,&ui.gizmo_handle);
    if(axis) {
      cancel_field_edit();SceneEditorMotionPlanPanelReset();
      ui.show_followers=false;ui.right_offset=0;
      ui.gizmo_axis=axis;ui.gizmo_initial=p->points[ui.point].position[axis-1];
      ui.handle=0;ui.drag=*p;ui.dragging=true;
      ui.revision=SceneEditorDocumentRevision();ui.down_x=x;ui.down_y=y;
      return true;
    }
    int point,handle;pick_point(p,x,y,&point,&handle);
    if (point >= 0) {
      cancel_field_edit();
      SceneEditorMotionPlanPanelReset();ui.show_followers=false;ui.right_offset=0;
      ui.point = point;
      ui.gizmo_axis=0;
      ui.handle = handle;
      ui.drag = *p;
      ui.dragging = handle != 0; /* Anchor clicks select; axis handles own movement. */
      ui.revision = SceneEditorDocumentRevision();
      ui.down_x = x;
      ui.down_y = y;
    }
    return true;
  }
  return hit(l->right_content_rect, x, y) ||
         (hit(l->left_content_rect, x, y) && y >= l->left_content_rect.y + 34);
}
bool SceneEditorMotionPathPanelEvent(SceneEditor *editor, SDL_Event *event,
                                     const SceneEditorPaneLayout *layout) {
  if(SceneEditorMotionPathPanelActive() && !SceneEditorMotionPlanPanelOpen() &&
      event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT) {
    SDL_Rect rect;
    if(SceneEditorMotionPlanPanelControl("plan_open",&rect) && hit(rect,event->button.x,event->button.y)) {
      cancel_field_edit();SceneEditorMotionPlanPanelReset();
      ui.show_followers=true;ui.right_offset=0;return true;
    }
  }
  if (SceneEditorMotionPathPanelActive() && SceneEditorMotionPlanPanelEvent(event)) {
    ui.editing = -1; ui.draft[0] = 0; ui.dragging = false; return true;
  }
  bool handled = motion_path_event(editor, event, layout);
  if (handled) {
    ui.message_revision = SceneEditorDocumentRevision();
    if(ui.message[0]) SceneEditorChromeShellSetActionFeedback(ui.message,5000);
  }
  return handled;
}
bool SceneEditorMotionPathPanelControl(const char *name, SDL_Rect *out) {
  if(MotionOrientationPanelControl(name,out))return true;
  if (!ui.active)
    return false;
  if ((ui.follower_type==0 && !strcmp(name,"path_timing")) ||
      (ui.follower_type==1 && !strcmp(name,"path_camera_timing")) ||
      (ui.follower_type==2 && !strcmp(name,"path_light_timing"))) {
    *out=ui.controls[FOLLOWER_TIMING];return out->w>0;
  }
  if (SceneEditorMotionPlanPanelControl(name, out)) return true;
  if (SceneEditorCameraPathPanelControl(name, out) || SceneEditorLightPathPanelControl(name, out)) return true;
  if(!strcmp(name,"path_smooth")) { *out=ui.controls[HANDLE_MODE];return out->w>0; }
  const char *axes[]={"path_gizmo_x","path_gizmo_y","path_gizmo_z"};
  for(int k=0;k<3;++k) if(!strcmp(name,axes[k])) { *out=ui.gizmo_controls[k];return out->w>0; }
  if (!strncmp(name, "path_row/", 9)) {
    MotionPaths paths;
    if (SceneEditorMotionPathsRead(&paths))
      for (size_t i = 0; i < paths.count; ++i)
        if (!strcmp(name + 9, paths.paths[i].id)) {
          *out = ui.rows[i];
          return out->w > 0;
        }
    return false;
  }
  if (!strncmp(name,"path_object/",12)) {
    for(int i=0;i<6;++i) if(!strcmp(name+12,ui.picker_ids[i])) { *out=ui.object_rows[i];return out->w>0; }
    return false;
  }
  if (!strncmp(name,"path_follower/",14)) {
    MotionPaths paths;
    if(SceneEditorMotionPathsRead(&paths)) for(size_t i=0;i<paths.binding_count;++i) {
      char target[160];const MotionPathBinding *b=&paths.bindings[i];
      snprintf(target,sizeof(target),b->target_id[0]?"%s":"object/%s",b->target_id[0]?b->target_id:b->object_id);
      if(!strcmp(name+14,target)) {*out=ui.follower_rows[i];return out->w>0;}
    }
    return false;
  }
  const char *names[] = {
      "new_path",         "path_name",       "path_previous",
      "path_next",        "path_add",        "path_remove",
      "path_segment",     "path_delete",     "follower_previous",
      "follower_next",    "path_attach",     "path_detach",
      "path_timing",      "path_save",       "path_frame",
      "path_select_tool", "path_place_tool", "path_frame_selected",
      "path_plane_z",     "path_followers",  "path_delete_selected", "path_handle_mode",
      "path_actions", "path_library_previous", "path_library_next",
      "path_follower_object", "path_follower_camera", "path_follower_light", "path_follower_back",
      "path_object_picker", "path_object_selected", "path_corner", "path_independent", "path_follower_plan", "path_restore_replan", "path_point_details", "path_follower_timing", "path_follow_direction", "path_forward_axis", "path_rotation_x", "path_rotation_y", "path_rotation_z", "path_rotation_from_base"};
  for (int i = 0; i < CONTROL_COUNT; ++i)
    if (!strcmp(name, names[i])) {
      *out = ui.controls[i];
      return out->w > 0;
    }
  const char *fields[] = {"path_x",     "path_y",     "path_z",
                          "path_in_x",  "path_in_y",  "path_in_z",
                          "path_out_x", "path_out_y", "path_out_z"};
  for (int i = 0; i < 9; ++i)
    if (!strcmp(name, fields[i])) {
      *out = ui.fields[i];
      return out->w > 0;
    }
  return false;
}
