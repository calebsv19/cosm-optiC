#ifndef SCENE_EDITOR_IDEAL_FOOTPRINT_NUMERIC_T4_H
#define SCENE_EDITOR_IDEAL_FOOTPRINT_NUMERIC_T4_H
#include "render/runtime_ray_3d.h"
#include <math.h>
#include <string.h>

static void ideal_t4_vector(Vec3 actual, Vec3 expected) {
    assert(vec3_length(vec3_sub(actual,expected)) <= 1e-8*fmax(1,vec3_length(expected)));
}
/* A single independent analytic plane, deliberately without a BVH or scene cache. */
static HitInfo3D ideal_t4_plane(Ray3D ray,Vec3 point,Vec3 normal) {
    normal=vec3_normalize(normal);
    Vec3 u=vec3_normalize(vec3_cross(normal,fabs(normal.z)<.9?vec3(0,0,1):vec3(0,1,0)));
    Vec3 v=vec3_cross(normal,u);
    RuntimeTriangle3D triangle={0};RuntimeScene3D scene={0};HitInfo3D hit;
    triangle.p0=vec3_add(point,vec3_add(vec3_scale(u,-100),vec3_scale(v,-100)));
    triangle.p1=vec3_add(point,vec3_add(vec3_scale(u,100),vec3_scale(v,-100)));
    triangle.p2=vec3_add(point,vec3_scale(v,100));triangle.normal=normal;
    scene.triangleMesh.triangles=&triangle;scene.triangleMesh.triangleCount=1;
    RuntimeRay3DTraceContext context;RuntimeRay3DTraceContext_Init(&context);
    RuntimeRay3DTraceContext_SetTraceRoute(&context,RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    assert(RuntimeRay3D_TraceSceneFirstHitWithContext(&context,&scene,&ray,1e-8,1e6,&hit));
    return hit;
}
static Vec3 ideal_t4_intersection(Vec3 origin,Vec3 direction,Vec3 point,Vec3 normal) {
    return vec3_add(origin,vec3_scale(direction,vec3_dot(vec3_sub(point,origin),normal)/vec3_dot(direction,normal)));
}
static void test_scene_editor_ideal_footprint_numeric_t4(void) {
    Ray3D primary=RuntimeRay3D_Make(vec3(0,0,3),vec3(.2,.1,-1));
    primary.hasDifferentials=true;
    primary.directionDx=vec3_normalize(vec3(.22,.1,-1));
    primary.directionDy=vec3_normalize(vec3(.2,.13,-1));
    Vec3 oblique=vec3_normalize(vec3(.3,-.2,1));
    Ray3D origin_ray=primary;origin_ray.hasDifferentialOrigins=true;
    origin_ray.originDx=vec3(.07,0,3);origin_ray.originDy=vec3(0,-.04,3);
    HitInfo3D oblique_hit=ideal_t4_plane(origin_ray,vec3(0,0,0),oblique);
    assert(oblique_hit.hasPixelFootprint && oblique_hit.constantShadingNormal);
    Vec3 center=ideal_t4_intersection(primary.origin,primary.direction,vec3(0,0,0),oblique);
    ideal_t4_vector(oblique_hit.pixelDpDx,vec3_sub(ideal_t4_intersection(origin_ray.originDx,primary.directionDx,vec3(0,0,0),oblique),center));
    ideal_t4_vector(oblique_hit.pixelDpDy,vec3_sub(ideal_t4_intersection(origin_ray.originDy,primary.directionDy,vec3(0,0,0),oblique),center));

    HitInfo3D first=ideal_t4_plane(primary,vec3(0,0,0),vec3(0,0,1));
    assert(!first.footprintTransported && !primary.footprintTransported);
    ideal_t4_vector(first.pixelDpDx,vec3(.06,0,0));
    ideal_t4_vector(first.pixelDpDy,vec3(0,.09,0));
    Ray3D reflected=RuntimeRay3D_Make(first.position,vec3(.2,.1,1));
    Vec3 saved_origin=reflected.origin,saved_direction=reflected.direction;
    assert(RuntimeRay3D_TransportIdealFootprint(&first,first.shadingNormal,RUNTIME_RAY_IDEAL_REFLECTION,1,1,&reflected));
    assert(!memcmp(&saved_origin,&reflected.origin,sizeof(Vec3)) && !memcmp(&saved_direction,&reflected.direction,sizeof(Vec3)));
    assert(reflected.footprintTransported);
    ideal_t4_vector(reflected.directionDx,vec3_normalize(vec3(.22,.1,1)));
    ideal_t4_vector(reflected.originDx,vec3_add(first.position,vec3(.06,0,0)));
    HitInfo3D second=ideal_t4_plane(reflected,vec3(0,0,2),vec3(0,0,-1));
    assert(second.footprintTransported);
    ideal_t4_vector(second.pixelDpDx,vec3(.10,0,0));
    Ray3D twice=RuntimeRay3D_Make(second.position,vec3(.2,.1,-1));
    assert(RuntimeRay3D_TransportIdealFootprint(&second,second.shadingNormal,RUNTIME_RAY_IDEAL_REFLECTION,1,1,&twice));
    HitInfo3D receiver=ideal_t4_plane(twice,vec3(0,0,-1),vec3(0,0,1));
    ideal_t4_vector(receiver.pixelDpDx,vec3(.16,0,0));
    ideal_t4_vector(receiver.pixelDpDy,vec3(0,.24,0));

    /* Independent Snell components: tangential direction / IOR, then unit Z. */
    Vec3 in=primary.direction, ix=primary.directionDx, iy=primary.directionDy;
    Vec3 snell=vec3(in.x/1.5,in.y/1.5,-sqrt(1-(in.x*in.x+in.y*in.y)/2.25));
    Vec3 snellx=vec3(ix.x/1.5,ix.y/1.5,-sqrt(1-(ix.x*ix.x+ix.y*ix.y)/2.25));
    Vec3 snelly=vec3(iy.x/1.5,iy.y/1.5,-sqrt(1-(iy.x*iy.x+iy.y*iy.y)/2.25));
    Ray3D refracted=RuntimeRay3D_Make(first.position,snell);
    assert(RuntimeRay3D_TransportIdealFootprint(&first,first.shadingNormal,RUNTIME_RAY_IDEAL_REFRACTION,1,1.5,&refracted));
    ideal_t4_vector(refracted.directionDx,snellx);ideal_t4_vector(refracted.directionDy,snelly);
    HitInfo3D exit_hit=ideal_t4_plane(refracted,vec3(0,0,-2),vec3(0,0,1));
    Ray3D exited=RuntimeRay3D_Make(exit_hit.position,in);
    assert(RuntimeRay3D_TransportIdealFootprint(&exit_hit,exit_hit.shadingNormal,RUNTIME_RAY_IDEAL_REFRACTION,1.5,1,&exited));
    ideal_t4_vector(exited.directionDx,ix);
    HitInfo3D after=ideal_t4_plane(exited,vec3(0,0,-5),vec3(0,0,1));
    Vec3 dx=vec3(.12+2*(snellx.x/-snellx.z-snell.x/-snell.z),2*(snellx.y/-snellx.z-snell.y/-snell.z),0);
    Vec3 dy=vec3(2*(snelly.x/-snelly.z-snell.x/-snell.z),.18+2*(snelly.y/-snelly.z-snell.y/-snell.z),0);
    ideal_t4_vector(after.pixelDpDx,dx);ideal_t4_vector(after.pixelDpDy,dy);

    Ray3D straight=RuntimeRay3D_Make(first.position,in);
    assert(RuntimeRay3D_TransportIdealFootprint(&first,first.shadingNormal,RUNTIME_RAY_IDEAL_STRAIGHT,1,1,&straight));
    ideal_t4_vector(straight.directionDx,ix);
    HitInfo3D bad=first;bad.constantShadingNormal=false;
    assert(!RuntimeRay3D_TransportIdealFootprint(&bad,bad.shadingNormal,RUNTIME_RAY_IDEAL_STRAIGHT,1,1,&straight));
    assert(straight.footprintUnbounded && !straight.hasDifferentials && !straight.hasDifferentialOrigins && !straight.footprintTransported);
    bad=first;bad.incidentDirectionDx=vec3(1,0,0);
    assert(!RuntimeRay3D_TransportIdealFootprint(&bad,bad.shadingNormal,RUNTIME_RAY_IDEAL_STRAIGHT,1,1,&straight));
    bad=first;bad.incidentDirectionDy=vec3(NAN,0,-1);
    assert(!RuntimeRay3D_TransportIdealFootprint(&bad,bad.shadingNormal,RUNTIME_RAY_IDEAL_STRAIGHT,1,1,&straight));
    bad=first;bad.incidentDirectionDx=vec3_normalize(vec3(.9,0,-.1));
    assert(!RuntimeRay3D_TransportIdealFootprint(&bad,bad.shadingNormal,RUNTIME_RAY_IDEAL_REFRACTION,1.5,1,&exited));
    bad=first;bad.hasUnperturbedShadingNormal=true;bad.unperturbedShadingNormal=vec3(0,.1,.99);
    assert(!RuntimeRay3D_TransportIdealFootprint(&bad,bad.shadingNormal,RUNTIME_RAY_IDEAL_STRAIGHT,1,1,&straight));
    origin_ray=primary;origin_ray.hasDifferentialOrigins=true;
    origin_ray.originDx=vec3(0,0,-1);origin_ray.originDy=primary.origin;
    bad=ideal_t4_plane(origin_ray,vec3(0,0,0),vec3(0,0,1));
    assert(bad.footprintUnbounded && !bad.hasPixelFootprint && !bad.footprintTransported);
    origin_ray=primary;origin_ray.directionDx=vec3(NAN,0,-1);
    bad=ideal_t4_plane(origin_ray,vec3(0,0,0),vec3(0,0,1));
    assert(bad.footprintUnbounded && !bad.hasPixelFootprint && !bad.footprintTransported);
}
#endif
