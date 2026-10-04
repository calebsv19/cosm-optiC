#include "editor/scene_editor_atmosphere_viewport.h"
#include "editor/scene_editor_atmosphere_geometry.h"
#include "editor/scene_editor_atmosphere_panel.h"
#include "editor/scene_editor_atmosphere_preview.h"
#include "editor/scene_editor_object_transform_handles.h"
#include "editor/scene_editor_transform_ergonomics.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_lifecycle.h"
#include "editor/scene_editor.h"
#include "editor/scene_editor_timeline.h"
#include "editor/object_editor.h"
#include "render/runtime_volume_3d_transform.h"
#include "editor/scene_editor_typography.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool selected,hover,proxy=true;
static char selected_path[4096];
static struct {
    bool active,angular;int x,y;double angle,accumulated;
    SceneEditorObjectTransformMode mode;SceneEditorBezier3DGizmoAxis axis;
    SceneEditorObjectTransformHandle handle;
    SceneAtmosphereAttachment original,preview;RuntimeVolumeAttachment3D geometry;
    Vec3 centre;unsigned long long revision;
} drag;
static SceneEditorBezier3DGizmoAxis hover_axis;
static void cancel(void) {if(drag.active) SDL_CaptureMouse(SDL_FALSE);memset(&drag,0,sizeof(drag));hover_axis=0;}
void SceneEditorAtmosphereViewportReset(void) {
    cancel();selected=hover=false;selected_path[0]=0;SceneEditorAtmosphereGeometryReset();
}
bool SceneEditorAtmosphereSelected(void) {
    SceneAtmosphereAttachment a;
    if(!selected || strcmp(selected_path,SceneEditorDocumentPath()) ||
       !SceneEditorAtmosphereRead(&a) || !a.present) {selected=false;cancel();}
    return selected;
}
void SceneEditorAtmosphereSelect(bool value) {
    cancel();selected=value;
    if(value) {
        snprintf(selected_path,sizeof(selected_path),"%s",SceneEditorDocumentPath());
        ObjectEditorSetSelectedObjectIndex(-1);SceneEditorObjectMoveGizmoReset();
    }
}
bool SceneEditorAtmosphereGestureActive(void) {return drag.active;}
bool SceneEditorAtmosphereDensityProxy(void) {return proxy;}
void SceneEditorAtmosphereDensityProxyToggle(void) {proxy=!proxy;}
static bool available(void) {
    return SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_ENVIRONMENT &&
        !SceneEditorAtmospherePreviewEnabled() && SceneEditorDocumentIsOpen() &&
        !SceneEditorAtmospherePanelInteractionActive() && !SceneEditorWorkspaceProfileMenuOpen() &&
        !SceneEditorLifecycleClosePending() && !RayTracingDeepRenderDesktopHost_HasActiveWork();
}
bool SceneEditorAtmospherePlacementPreview(SceneAtmosphereAttachment* out) {
    if(!out || !drag.active || !available() || drag.revision!=SceneEditorDocumentRevision()) return false;
    *out=drag.preview;return true;
}
static void corners(const RuntimeVolumeAttachment3D* v,Vec3 points[8]) {
    for(int i=0;i<8;++i) points[i]=RuntimeVolume3D_ToWorldPoint(v,
        vec3(i&1?v->grid.boundsMax.x:v->grid.boundsMin.x,
            i&2?v->grid.boundsMax.y:v->grid.boundsMin.y,i&4?v->grid.boundsMax.z:v->grid.boundsMin.z));
}
static double segment_distance(int x,int y,int ax,int ay,int bx,int by) {
    double dx=bx-ax,dy=by-ay,length=dx*dx+dy*dy;
    double t=length>0?fmax(0,fmin(1,((x-ax)*dx+(y-ay)*dy)/length)):0;
    return hypot(x-ax-t*dx,y-ay-t*dy);
}
static bool pick_bounds(const RuntimeVolumeAttachment3D* v,const SceneEditorDigestOverlayProjector* p,int x,int y) {
    Vec3 points[8];corners(v,points);int sx[8],sy[8];bool valid[8];
    for(int i=0;i<8;++i) valid[i]=SceneEditorDigestOverlayProjectPoint(p,points[i].x,points[i].y,points[i].z,&sx[i],&sy[i]);
    for(int i=0;i<8;++i) for(int bit=1;bit<=4;bit*=2) if(!(i&bit) && valid[i] && valid[i|bit] &&
        segment_distance(x,y,sx[i],sy[i],sx[i|bit],sy[i|bit])<8) return true;
    Vec3 c=SceneEditorAtmosphereCentre(v);int cx,cy;
    return SceneEditorDigestOverlayProjectPoint(p,c.x,c.y,c.z,&cx,&cy) && hypot(x-cx,y-cy)<12;
}
bool SceneEditorAtmosphereFrame(void) {
    RuntimeVolumeAttachment3D v;RuntimeSceneBridge3DDigestState digest;SceneEditorPaneLayout layout;
    SceneEditorDigestOverlayProjector p;SceneEditorDigestOverlayNavState state=*SceneEditorGetViewportNavState();
    SceneEditorDigestOverlayNavState* nav=&state;
    if(!SceneEditorAtmosphereGeometry(&v,NULL,0) || !SceneEditorDigestOverlayResolve(&digest) ||
       !SceneEditorGetPaneLayout(&layout) || !SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,nav,&p)) return false;
    Vec3 c=SceneEditorAtmosphereCentre(&v),points[8];corners(&v,points);
    p.center_x=c.x;p.center_y=c.y;p.center_z=c.z;
    double width=0,height=0,cx,cy;SceneEditorDigestOverlayProjectPointF(&p,c.x,c.y,c.z,&cx,&cy);
    for(int i=0;i<8;++i) {double x,y;if(SceneEditorDigestOverlayProjectPointF(&p,points[i].x,points[i].y,points[i].z,&x,&y)) {
        width=fmax(width,2*fabs(x-cx));height=fmax(height,2*fabs(y-cy));}}
    if(width<1 || height<1) return false;
    nav->overlay_zoom*=.65*fmin(layout.viewport_rect.w/width,layout.viewport_rect.h/height);
    nav->target_valid=true;nav->target_x=c.x;nav->target_y=c.y;nav->target_z=c.z;
    nav->zoom_limits_valid=false;SceneEditorRestoreViewportNav(nav);SceneEditorAtmospherePreviewSetEnabled(false);return true;
}
static void update(int x,int y) {
    drag.preview=drag.original;int component=(int)drag.axis-1;
    double pixels=(x-drag.x)*drag.handle.ux+(y-drag.y)*drag.handle.uy;
    if(drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE) {
        double w=SceneEditorDocumentWorldScale();
        double d=SceneEditorTransformSnapValue(drag.mode,pixels/(drag.handle.pixels_per_unit*w))*w;
        Vec3 c=drag.centre;if(component==0)c.x+=d;else if(component==1)c.y+=d;else c.z+=d;
        SceneEditorAtmosphereSetCentre(&drag.preview,&drag.geometry,c,SceneEditorDocumentWorldScale());
    } else {
        if(drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_SCALE) {
            double factor=SceneEditorTransformSnapValue(drag.mode,exp(fmax(-20,fmin(20,pixels/96))));
            for(int i=0;i<3;++i) if(component==i || drag.axis==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM)
                drag.preview.scale[i]=fmax(1e-6,drag.original.scale[i]*factor);
        } else {
            double angle;
            if(drag.angular && SceneEditorObjectTransformHandleAngle(&drag.handle,x,y,&angle)) {
                drag.accumulated+=remainder(angle-drag.angle,6.283185307179586);drag.angle=angle;
            } else if(!drag.angular) drag.accumulated=((x-drag.x)-(y-drag.y))*.0174532925199433;
            drag.preview.rotation_degrees[component]+=SceneEditorTransformSnapValue(drag.mode,drag.accumulated*57.29577951308232);
        }
        SceneEditorAtmosphereSetCentre(&drag.preview,&drag.geometry,drag.centre,SceneEditorDocumentWorldScale());
    }
}
bool SceneEditorAtmosphereViewportEvent(const SDL_Event* e,SDL_Window* window) {
    if(!e || !window) return false;
    Uint32 id=e->type==SDL_MOUSEMOTION?e->motion.windowID:
        e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEBUTTONUP?e->button.windowID:
        e->type==SDL_KEYDOWN || e->type==SDL_KEYUP?e->key.windowID:
        e->type==SDL_WINDOWEVENT?e->window.windowID:0;
    if(id && id!=SDL_GetWindowID(window)) return false;
    if(e->type==SDL_QUIT || (e->type==SDL_WINDOWEVENT &&
       (e->window.event==SDL_WINDOWEVENT_FOCUS_LOST || e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED || e->window.event==SDL_WINDOWEVENT_CLOSE))) {cancel();return false;}
    bool owns=SceneEditorAtmosphereSelected();
    if(drag.active && (!available() || !owns || drag.revision!=SceneEditorDocumentRevision() || drag.mode!=SceneEditorObjectTransformModeGet())) {
        cancel();SceneEditorChromeShellSetActionFeedback("Atmosphere transform cancelled: editing context changed",2500);
        return e->type==SDL_MOUSEMOTION || e->type==SDL_MOUSEBUTTONUP;
    }
    if(drag.active) {
        if(e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_ESCAPE) {cancel();return true;}
        if(e->type==SDL_MOUSEMOTION) {update(e->motion.x,e->motion.y);return true;}
        if(e->type==SDL_MOUSEBUTTONUP && e->button.button==SDL_BUTTON_LEFT) {
            update(e->button.x,e->button.y);char d[512];
            bool moved=memcmp(drag.preview.translation,drag.original.translation,sizeof(drag.original.translation)) ||
                memcmp(drag.preview.rotation_degrees,drag.original.rotation_degrees,sizeof(drag.original.rotation_degrees)) ||
                memcmp(drag.preview.scale,drag.original.scale,sizeof(drag.original.scale));
            bool ok=!moved || SceneEditorAtmosphereSet(&drag.preview,drag.revision,d,sizeof(d));
            cancel();SceneEditorChromeShellSetActionFeedback(ok?"Atmosphere transform applied; Undo available":d,3000);return true;
        }
        if(e->type==SDL_DROPFILE) {SDL_free(e->drop.file);return true;}
        return e->type==SDL_KEYDOWN || e->type==SDL_KEYUP || e->type==SDL_TEXTINPUT || e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEWHEEL;
    }
    if(!available()) {hover=false;hover_axis=0;return false;}
    if(owns && e->type==SDL_KEYDOWN && !e->key.repeat && !(e->key.keysym.mod&(KMOD_CTRL|KMOD_GUI|KMOD_ALT))) {
        SDL_Keycode key=e->key.keysym.sym;
        if(key==SDLK_f) return SceneEditorAtmosphereFrame();
        if(key==SDLK_w || key==SDLK_r || key==SDLK_e) {
            SceneEditorObjectTransformModeSet(key==SDLK_w?SCENE_EDITOR_OBJECT_TRANSFORM_MOVE:key==SDLK_r?SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE:SCENE_EDITOR_OBJECT_TRANSFORM_SCALE);return true;
        }
    }
    bool motion=e->type==SDL_MOUSEMOTION;
    if(!motion && (e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT)) return false;
    int x=motion?e->motion.x:e->button.x,y=motion?e->motion.y:e->button.y;
    SceneEditorPaneLayout layout;RuntimeSceneBridge3DDigestState digest;SceneEditorDigestOverlayProjector p;RuntimeVolumeAttachment3D v;
    hover=false;hover_axis=0;
    if(SDL_GetModState()&(KMOD_ALT|KMOD_CTRL|KMOD_GUI) || !SceneEditorGetPaneLayout(&layout) ||
       !SDL_PointInRect(&(SDL_Point){x,y},&layout.viewport_rect) || !SceneEditorAtmosphereGeometry(&v,NULL,0) ||
       !SceneEditorDigestOverlayResolve(&digest) || !SceneEditorDigestOverlayBuildProjector(&digest,&layout.viewport_rect,SceneEditorGetViewportNavState(),&p)) return false;
    Vec3 c=SceneEditorAtmosphereCentre(&v);double centre[3]={c.x,c.y,c.z};SceneEditorObjectTransformHandle h;
    SceneEditorBezier3DGizmoAxis axis;
    if(owns && ObjectEditorTransformHandlesVisible() && SceneEditorObjectTransformHandlePick(&p,&digest,centre,SceneEditorObjectTransformModeGet(),x,y,&axis,&h)) {
        hover_axis=axis;if(motion) return false;
        SceneEditorAtmosphereRead(&drag.original);drag.preview=drag.original;drag.geometry=v;drag.centre=c;
        drag.mode=SceneEditorObjectTransformModeGet();drag.handle=h;drag.axis=axis;drag.x=x;drag.y=y;
        drag.angular=drag.mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE && SceneEditorObjectTransformHandleAngle(&h,x,y,&drag.angle);
        SceneEditorTimelinePause();drag.revision=SceneEditorDocumentRevision();drag.active=true;SDL_CaptureMouse(SDL_TRUE);return true;
    }
    hover=pick_bounds(&v,&p,x,y);
    if(!motion) {SceneEditorAtmosphereSelect(hover);return hover;}
    return false;
}
void SceneEditorAtmosphereViewportDraw(SDL_Renderer* r,const SceneEditorDigestOverlayProjector* p,const RuntimeSceneBridge3DDigestState* digest) {
    if(!r || SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_ENVIRONMENT) return;
    RuntimeVolumeAttachment3D v;SceneAtmosphereAttachment a;
    if(!SceneEditorAtmosphereGeometry(&v,NULL,0) || !SceneEditorAtmosphereRead(&a)) return;
    bool owns=SceneEditorAtmosphereSelected();
    if(drag.active && (!available() || drag.revision!=SceneEditorDocumentRevision())) cancel();
    if(drag.active) SceneEditorAtmospherePlaceGeometry(&v,&drag.preview,SceneEditorDocumentWorldScale());
    Vec3 points[8];corners(&v,points);int x[8],y[8];bool valid[8];
    for(int i=0;i<8;++i) valid[i]=SceneEditorDigestOverlayProjectPoint(p,points[i].x,points[i].y,points[i].z,&x[i],&y[i]);
    SDL_Color color=owns?(SDL_Color){255,210,90,255}:hover?(SDL_Color){170,235,255,255}:(SDL_Color){90,175,215,190};
    if(!a.enabled || !a.visible) color=(SDL_Color){130,145,155,170};
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    for(int i=0;i<8;++i) for(int bit=1;bit<=4;bit*=2) if(!(i&bit) && valid[i] && valid[i|bit]) {
        SDL_RenderDrawLine(r,x[i],y[i],x[i|bit],y[i|bit]);
        if(owns || hover) SDL_RenderDrawLine(r,x[i]+1,y[i],x[i|bit]+1,y[i|bit]);
    }
    /* Bounded density stipple: an editing aid, never a shaded cloud claim. */
    if(proxy && a.enabled && a.visible && v.channels.density) {
        const uint32_t dims[3]={v.grid.gridW,v.grid.gridH,v.grid.gridD};
        for(int z=0;z<8;++z) for(int yv=0;yv<8;++yv) for(int xv=0;xv<8;++xv) {
            uint32_t ix=(uint32_t)((xv+.5)*dims[0]/8),iy=(uint32_t)((yv+.5)*dims[1]/8),iz=(uint32_t)((z+.5)*dims[2]/8);
            float density=v.channels.density[((uint64_t)iz*dims[1]+iy)*dims[0]+ix];
            if(density*a.density_scale<=1e-5) continue;
            Vec3 q=RuntimeVolume3D_ToWorldPoint(&v,vec3_add(v.grid.origin,vec3((ix+.5)*v.grid.voxelSize,(iy+.5)*v.grid.voxelSize,(iz+.5)*v.grid.voxelSize)));
            int sx,sy;if(SceneEditorDigestOverlayProjectPoint(p,q.x,q.y,q.z,&sx,&sy)) {
                int radius=density*a.density_scale>1?3:2;
                SDL_Rect dot={sx-radius,sy-radius,radius*2,radius*2};
                SDL_SetRenderDrawColor(r,120,205,230,100);SDL_RenderDrawRect(r,&dot);
            }
        }
    }
    Vec3 c=SceneEditorAtmosphereCentre(&v);int cx,cy;
    if(SceneEditorDigestOverlayProjectPoint(p,c.x,c.y,c.z,&cx,&cy)) {
        SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);SDL_Rect marker={cx-5,cy-5,11,11};SDL_RenderDrawRect(r,&marker);
        SceneEditorLabelLeft(r,(SDL_Rect){cx+12,cy-26,260,22},a.enabled?"Atmosphere | density guide":"Atmosphere | disabled",color);
    }
    if(owns && ObjectEditorTransformHandlesVisible()) {double centre[3]={c.x,c.y,c.z};SceneEditorObjectTransformHandlesRender(r,p,digest,centre,SceneEditorObjectTransformModeGet(),hover_axis,drag.active?drag.axis:0);}
}
