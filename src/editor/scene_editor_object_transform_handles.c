#include "editor/scene_editor_object_transform_handles.h"
#include "editor/scene_editor_digest_overlay_internal.h"
#include "editor/scene_editor_typography.h"
#include <math.h>

static const double tau=6.2831853071795864769;
static void ring_point(const SceneEditorObjectTransformHandle* h,double t,double* x,double* y) {
    *x=h->cx+cos(t)*h->ux+sin(t)*h->vx;
    *y=h->cy+cos(t)*h->uy+sin(t)*h->vy;
}
bool SceneEditorObjectTransformHandleProject(const SceneEditorDigestOverlayProjector* p,
    const RuntimeSceneBridge3DDigestState* digest,const double position[3],
    SceneEditorObjectTransformMode mode,SceneEditorBezier3DGizmoAxis axis,
    SceneEditorObjectTransformHandle* h) {
    if (!p || !digest || !h || axis<1 || axis>SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM) return false;
    *h=(SceneEditorObjectTransformHandle){0};
    if (axis==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM) {
        if (mode!=SCENE_EDITOR_OBJECT_TRANSFORM_SCALE ||
            !SceneEditorDigestOverlayProjectPointF(p,position[0],position[1],position[2],&h->cx,&h->cy)) return false;
        h->x=(int)lround(h->cx)+14;h->y=(int)lround(h->cy)+14;
        h->ux=0.7071067811865476;h->uy=0.7071067811865476;h->pixels_per_unit=p->scale;
        return true;
    }
    if (mode!=SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE) {
        int ax,ay,bx,by; double ppu;
        SceneEditorBezier3DInteractionMetrics metrics=SceneEditorDigestOverlayResolveBezierMetrics(digest,p);
        if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(p,position[0],position[1],position[2],
            axis,metrics.gizmo_world_length,&ax,&ay,&bx,&by,&ppu) || ppu<=0) return false;
        if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(p,position[0],position[1],position[2],
            axis,72.0/ppu,&ax,&ay,&bx,&by,&ppu)) return false;
        double length=hypot((double)bx-ax,(double)by-ay);
        if (length<8) return false;
        h->cx=ax;h->cy=ay;h->x=bx;h->y=by;
        h->ux=(bx-ax)/length;h->uy=(by-ay)/length;h->pixels_per_unit=ppu;
        return true;
    }
    if (p->scale<=0) return false;
    int component=(int)axis-1;
    double u[3]={position[0],position[1],position[2]},v[3]={position[0],position[1],position[2]};
    u[(component+1)%3]+=64.0/p->scale;
    v[(component+2)%3]+=64.0/p->scale;
    double ux,uy,vx,vy;
    if (!SceneEditorDigestOverlayProjectPointF(p,position[0],position[1],position[2],&h->cx,&h->cy) ||
        !SceneEditorDigestOverlayProjectPointF(p,u[0],u[1],u[2],&ux,&uy) ||
        !SceneEditorDigestOverlayProjectPointF(p,v[0],v[1],v[2],&vx,&vy)) return false;
    h->ux=ux-h->cx;h->uy=uy-h->cy;h->vx=vx-h->cx;h->vy=vy-h->cy;
    double x,y;ring_point(h,0.45+component*1.9,&x,&y);
    h->x=(int)lround(x);h->y=(int)lround(y);
    return true;
}
bool SceneEditorObjectTransformHandleAngle(const SceneEditorObjectTransformHandle* h,
    int x,int y,double* angle) {
    double determinant=h->ux*h->vy-h->uy*h->vx;
    if (fabs(determinant)<32.0) return false; /* Edge-on rings use a linear drag fallback. */
    double dx=x-h->cx,dy=y-h->cy;
    double u=(dx*h->vy-dy*h->vx)/determinant;
    double v=(dy*h->ux-dx*h->uy)/determinant;
    if (hypot(u,v)<0.1) return false;
    *angle=atan2(v,u);return true;
}
bool SceneEditorObjectTransformHandlePick(const SceneEditorDigestOverlayProjector* p,
    const RuntimeSceneBridge3DDigestState* digest,const double position[3],
    SceneEditorObjectTransformMode mode,int x,int y,
    SceneEditorBezier3DGizmoAxis* axis,SceneEditorObjectTransformHandle* picked) {
    double best=100.0;bool found=false;
    int last=mode==SCENE_EDITOR_OBJECT_TRANSFORM_SCALE ? SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM : 3;
    for (int i=1;i<=last;++i) {
        SceneEditorObjectTransformHandle h;
        if (!SceneEditorObjectTransformHandleProject(p,digest,position,mode,(SceneEditorBezier3DGizmoAxis)i,&h)) continue;
        double distance=(x-h.x)*(double)(x-h.x)+(y-h.y)*(double)(y-h.y);
        /* Labeled endpoints take precedence over intersecting rings. */
        if (i==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM && distance<100) distance*=0.0001;
        else if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE && distance<64) distance*=0.001;
        else if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE) {
            distance=1e12;
            for (int j=0;j<96;++j) {
                double px,py;ring_point(&h,tau*j/96.0,&px,&py);
                double d=(x-px)*(x-px)+(y-py)*(y-py);
                if (d<distance) distance=d;
            }
        }
        if (distance>=best) continue;
        best=distance;*axis=(SceneEditorBezier3DGizmoAxis)i;*picked=h;found=true;
    }
    return found;
}
void SceneEditorObjectTransformHandlesRender(SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* p,const RuntimeSceneBridge3DDigestState* digest,
    const double position[3],SceneEditorObjectTransformMode mode,
    SceneEditorBezier3DGizmoAxis hover,SceneEditorBezier3DGizmoAxis active) {
    static const SDL_Color colors[3]={{235,105,105,255},{100,215,135,255},{105,155,240,255}};
    static const char* labels[]={"X","Y","Z","All"};
    int last=mode==SCENE_EDITOR_OBJECT_TRANSFORM_SCALE ? SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM : 3;
    for (int i=1;i<=last;++i) {
        SceneEditorObjectTransformHandle h;
        if (!SceneEditorObjectTransformHandleProject(p,digest,position,mode,(SceneEditorBezier3DGizmoAxis)i,&h)) continue;
        SDL_Color color=(int)active==i ? (SDL_Color){255,220,115,255} : (int)hover==i ? (SDL_Color){255,255,255,255} :
            i==SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM ? (SDL_Color){210,210,215,255} : colors[i-1];
        SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,255);
        if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE) {
            SDL_Point points[97];
            for (int j=0;j<=96;++j) { double x,y;ring_point(&h,tau*j/96.0,&x,&y);points[j]=(SDL_Point){(int)lround(x),(int)lround(y)}; }
            for (int j=1;j<=96;++j) SDL_RenderDrawLine(renderer,points[j-1].x,points[j-1].y,points[j].x,points[j].y);
        } else if (i!=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_UNIFORM) {
            SDL_RenderDrawLine(renderer,(int)h.cx,(int)h.cy,h.x,h.y);
            SDL_RenderDrawLine(renderer,(int)h.cx+1,(int)h.cy,h.x+1,h.y);
        }
        SDL_Rect handle={h.x-5,h.y-5,11,11};
        if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_SCALE) SDL_RenderDrawRect(renderer,&handle);
        else SDL_RenderFillRect(renderer,&handle);
        if ((int)active==i || (int)hover==i) { SDL_Rect ring={h.x-8,h.y-8,17,17};SDL_RenderDrawRect(renderer,&ring); }
        SceneEditorLabel(renderer,(SDL_Rect){h.x+9,h.y-10,i==4 ? 34 : 20,20},labels[i-1],color);
    }
}
