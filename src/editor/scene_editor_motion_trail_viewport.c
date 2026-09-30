#include "editor/scene_editor_motion_trail.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_motion_paths.h"
#include "scene_editor_object_timeline_panel.h"
#include "scene_editor_timeline_commands.h"
#include "scene_editor_motion_path_viewport.h"
#include "editor/scene_editor_render_authoring.h"
#include <stdlib.h>
#include "import/runtime_scene_object_timeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static struct {
  SceneEditorMotionTrail trail;
  SceneEditorDigestOverlayProjector projector;
  bool visible,dragging,moved;
  SDL_Rect frame_button;
  size_t selected;
  double draft[3];int down_x,down_y;
  char labels[MOTION_TRAIL_KEY_CAPACITY][32],message[160];
} view;
void SceneEditorMotionTrailReset(void){memset(&view,0,sizeof(view));}
bool SceneEditorMotionTrailControl(const char *name,SDL_Rect *out) {
  if(!strcmp(name,"frame_xyz_trail") && view.visible){*out=view.frame_button;return out->w>0;}return false;
}
static bool resolve(const SceneEditorPaneLayout *layout) {
  TimelineTrack selected;TimelineRate rate;TimelineRange range;TimelineSample sample;
  if(SceneEditorMotionPathPanelActive() || layout->viewport_expanded ||
     !SceneEditorTimelineSelectedTrack(&selected,&rate,&range,&sample) || RuntimeObjectTimelineAxis(selected.property_id)<0) {view.visible=false;view.dragging=false;return false;}
  if(view.dragging && (strcmp(view.trail.target,selected.target_id) || view.trail.revision!=SceneEditorDocumentRevision()))view.dragging=false;
  if(!view.dragging && !SceneEditorMotionTrailRead(selected.target_id,&view.trail)){view.visible=false;return false;}
  RuntimeSceneBridge3DDigestState digest;
  view.visible=SceneEditorDigestOverlayResolve(&digest) && SceneEditorDigestOverlayBuildProjector(&digest,&layout->viewport_rect,SceneEditorGetViewportNavState(),&view.projector);
  return view.visible;
}
static bool project(const double p[3],int *x,int *y) {
  double scale=SceneEditorDocumentWorldScale();return SceneEditorDigestOverlayProjectPoint(&view.projector,p[0]*scale,p[1]*scale,p[2]*scale,x,y);
}
void SceneEditorMotionTrailDraw(SceneEditor *editor,const SceneEditorPaneLayout *layout) {
  if(!resolve(layout))return;SDL_Renderer *r=editor->renderer;
  SDL_Rect prior;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&prior);SDL_RenderSetClipRect(r,&layout->viewport_rect);
  SDL_SetRenderDrawColor(r,85,205,230,255);
  for(size_t i=1;i<view.trail.count;++i) {
    double from=view.trail.frames[i-1],to=view.trail.frames[i],p[3];int px=0,py=0;bool previous=false;
    for(int step=0;step<=24;++step) {
      double frame=from+(to-from)*step/24;int x,y;
      /* Never draw a fictitious travel line across a Step jump. */
      bool jump=false;for(int a=0;a<3 && step==24;++a)for(size_t k=1;k<view.trail.axes[a].key_count;++k) {
        TimelineKeyframe *l=&view.trail.axes[a].keys[k-1],*right=&view.trail.axes[a].keys[k];
        if(right->frame==to && l->interpolation_to_next==TIMELINE_INTERPOLATION_STEP && l->value.as.scalar!=right->value.as.scalar)jump=true;
      }
      bool valid=SceneEditorMotionTrailSample(&view.trail,frame,p) && project(p,&x,&y);
      if(valid && previous && !jump)SDL_RenderDrawLine(r,px,py,x,y);
      previous=valid;if(valid){px=x;py=y;}
    }
  }
  for(size_t i=0;i<view.trail.count;++i) {
    double p[3];int x,y;if(!SceneEditorMotionTrailSample(&view.trail,view.trail.frames[i],p))continue;
    if(view.dragging && i==view.selected)memcpy(p,view.draft,sizeof(p));
    if(!project(p,&x,&y))continue;
    SceneEditorTimelineKeySelection keys;bool selected=SceneEditorTimelineSelectionRead(&keys) && keys.primary.frame==view.trail.frames[i];
    SDL_SetRenderDrawColor(r,selected?255:245,selected?245:185,selected?220:75,255);
    int radius=selected?7:5;SDL_Rect marker={x-radius,y-radius,2*radius,2*radius};SDL_RenderFillRect(r,&marker);
    snprintf(view.labels[i],sizeof(view.labels[i]),"%lld",(long long)view.trail.frames[i]);
    ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,10,8),view.labels[i],x+7,y-12,(SDL_Color){245,210,130,255});
  }
  view.frame_button=(SDL_Rect){layout->viewport_rect.x+8,layout->viewport_rect.y+6,150,26};
  SceneEditorRenderButton(r,view.frame_button,"Frame XYZ trail",false,true);
  ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,10,8),view.message[0]?view.message:"Drag: XY | Z: inspector | Esc: cancel",layout->viewport_rect.x+8,layout->viewport_rect.y+36,(SDL_Color){180,225,240,255});
  SDL_RenderSetClipRect(r,clipped?&prior:NULL);
}
bool SceneEditorMotionTrailEvent(SceneEditor *editor,SDL_Event *e,const SceneEditorPaneLayout *layout) {
  (void)editor;if(!resolve(layout))return false;
  if(view.dragging) {
    if((e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_ESCAPE) || e->type==SDL_WINDOWEVENT || (SDL_GetModState()&(KMOD_ALT|KMOD_CTRL|KMOD_GUI))) {view.dragging=false;return true;}
    if(e->type==SDL_MOUSEMOTION) {
      double x,y,z,scale=SceneEditorDocumentWorldScale();
      if(SceneEditorDigestOverlayScreenRayToPlanePoint(&view.projector,e->motion.x,e->motion.y,view.draft[2]*scale,&x,&y,&z)) {
        view.draft[0]=x/scale;view.draft[1]=y/scale;
        view.moved=abs(e->motion.x-view.down_x)+abs(e->motion.y-view.down_y)>3;
      }return true;
    }
    if(e->type==SDL_MOUSEBUTTONUP && e->button.button==SDL_BUTTON_LEFT) {
      view.dragging=false;
      if(view.moved && SceneEditorMotionTrailSetKey(view.trail.target,view.trail.frames[view.selected],view.draft,view.trail.revision,view.message,sizeof(view.message)))snprintf(view.message,sizeof(view.message),"XYZ key updated together. Undo restores all axes.");
      return true;
    }
  }
  if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT || (SDL_GetModState()&(KMOD_ALT|KMOD_CTRL|KMOD_GUI|KMOD_SHIFT)))return false;
  SDL_Point point={e->button.x,e->button.y};if(!SDL_PointInRect(&point,&layout->viewport_rect))return false;
  if(SDL_PointInRect(&point,&view.frame_button)) {
    MotionPath bounds={.count=2};bool first=true;
    for(size_t i=0;i<view.trail.count;++i)for(int step=0;step<=24;++step){
      double frame=view.trail.frames[i];if(i)frame=view.trail.frames[i-1]+(frame-view.trail.frames[i-1])*step/24;
      double p[3];if(!SceneEditorMotionTrailSample(&view.trail,frame,p))continue;
      for(int a=0;a<3;++a){bounds.points[0].position[a]=first?p[a]:fmin(bounds.points[0].position[a],p[a]);bounds.points[1].position[a]=first?p[a]:fmax(bounds.points[1].position[a],p[a]);}first=false;
    }
    if(!first)MotionPathViewportFrame(&bounds,&layout->viewport_rect);return true;
  }
  double best=11;size_t found=SIZE_MAX;
  for(size_t i=0;i<view.trail.count;++i){double p[3];int x,y;if(SceneEditorMotionTrailSample(&view.trail,view.trail.frames[i],p) && project(p,&x,&y)){double d=hypot(x-point.x,y-point.y);if(d<best){best=d;found=i;}}}
  if(found==SIZE_MAX)return false;
  SceneEditorObjectTimelinePanelReset();SceneEditorTimelinePause();SceneEditorTimelineSeek(view.trail.frames[found]);
  const TimelineDocument *doc=SceneEditorTimelineDocumentView(NULL);
  for(size_t i=0;doc && i<doc->track_count;++i)if(doc->tracks[i].enabled && !strcmp(doc->tracks[i].target_id,view.trail.target) && RuntimeObjectTimelineAxis(doc->tracks[i].property_id)>=0) {
    bool key=false;for(size_t k=0;k<doc->tracks[i].key_count;++k)if(doc->tracks[i].keys[k].frame==view.trail.frames[found])key=true;
    if(key){SceneEditorTimelineSelectTrack(i);SceneEditorTimelineSelectKey(view.trail.frames[found],false);break;}
  }
  view.selected=found;view.dragging=true;view.moved=false;view.down_x=point.x;view.down_y=point.y;
  SceneEditorMotionTrailSample(&view.trail,view.trail.frames[found],view.draft);return true;
}
