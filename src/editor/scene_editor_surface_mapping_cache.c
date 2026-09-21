#include "editor/scene_editor_surface_mapping_cache.h"
#include "editor/scene_editor_material_stack.h"
#include "editor/scene_editor_document.h"
#include "render/runtime_material_payload_3d.h"
#include "config/config_manager.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define WIDTH 512
#define HEIGHT 256
#define LEVELS 10
#define PIXELS 174763
#define SLOTS 4
/* Camera-independent chart cache, ~22 MiB including mip levels. */
typedef struct Channels { float v[8]; } Channels;
struct SceneEditorSurfaceMappingCache {
    bool valid;int index;uint64_t revision,hash,stamp;
    CoreAuthoredSurfaceMapping map;
    RuntimeMaterialSurfaceEval base;
    double repeats,vmin,vspan,world_scale;
    Vec3 tangent;
    Channels samples[PIXELS];
};
static SceneEditorSurfaceMappingCache slots[SLOTS];
static uint64_t builds,tick;
static Channels channels(const RuntimeMaterialSurfaceEval* e) {
    return (Channels){{e->colorR,e->colorG,e->colorB,e->roughness,e->reflectivity,e->specWeight,e->diffuseWeight,e->transparency}};
}
static RuntimeMaterialSurfaceEval eval(Channels c) {
    return RuntimeMaterialSurfaceEvalMakeBase(c.v[0],c.v[1],c.v[2],c.v[3],c.v[4],c.v[5],c.v[6],c.v[7]);
}
static uint64_t hash_bytes(uint64_t h,const void* p,size_t n) {
    const unsigned char* b=p;for(size_t i=0;i<n;++i) h=(h^b[i])*UINT64_C(1099511628211);return h;
}
const SceneEditorSurfaceMappingCache* SceneEditorSurfaceMappingCachePrepare(int index) {
    CoreAuthoredSurfaceMapping map;
    RuntimeMaterialPayload3D payload;
    RuntimeMaterialTextureStack stack;
    if(RuntimeSurfaceMappingNeedsMeshAttributes(index) || !RuntimeSurfaceMappingPreviewSupported(index) || !RuntimeSurfaceMappingDefinition(index,&map) || map.version!=2 ||
       !RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(index,&payload) ||
       !SceneEditorMaterialStackGetEffectiveObjectStack(&sceneSettings.sceneObjects[index],index,&stack)) return NULL;
    RuntimeMaterialSurfaceEval base=RuntimeMaterialSurfaceEvalMakeBase(payload.baseColorR,payload.baseColorG,payload.baseColorB,
        payload.bsdf.roughness,payload.bsdf.reflectivity,payload.bsdf.specWeight,payload.bsdf.diffuseWeight,payload.transparency);
    uint64_t h=hash_bytes(0,&stack,sizeof(stack));h=hash_bytes(h,&base,sizeof(base));
    uint64_t rev=RuntimeSurfaceMappingRevision();
    SceneEditorSurfaceMappingCache* slot=&slots[0];
    for(int i=0;i<SLOTS;++i) {
        if(slots[i].valid && slots[i].index==index && slots[i].revision==rev && slots[i].hash==h) {slots[i].stamp=++tick;return &slots[i];}
        if(slots[i].stamp<slot->stamp) slot=&slots[i];
    }
    slot->valid=false;slot->index=index;slot->revision=rev;slot->hash=h;slot->stamp=++tick;slot->map=map;slot->base=base;
    slot->world_scale=SceneEditorDocumentWorldScale();
    slot->tangent=vec3(map.axis_v[1]*map.axis_u[2]-map.axis_v[2]*map.axis_u[1],
        map.axis_v[2]*map.axis_u[0]-map.axis_v[0]*map.axis_u[2],map.axis_v[0]*map.axis_u[1]-map.axis_v[1]*map.axis_u[0]);
    slot->repeats=round(6.2831853071795864769*map.reference_radius_m/map.tile_m[0]);
    slot->vmin=(map.height_range_m[0]+map.offset_m[1])/map.tile_m[1];
    slot->vspan=(map.height_range_m[1]-map.height_range_m[0])/map.tile_m[1];
    for(int y=0;y<HEIGHT;++y) for(int x=0;x<WIDTH;++x) {
        RuntimeMaterialSurfaceEval e;
        if(!RuntimeSurfaceMappingEvaluateTiles(index,(x+.5)*slot->repeats/WIDTH,
            slot->vmin+(y+.5)*slot->vspan/HEIGHT,&base,&e)) return NULL;
        slot->samples[y*WIDTH+x]=channels(&e);
    }
    int previous=0,offset=WIDTH*HEIGHT,w=WIDTH,hg=HEIGHT;
    while(w>1 || hg>1) {
        int nw=w>1?w/2:1,nh=hg>1?hg/2:1;
        for(int y=0;y<nh;++y) for(int x=0;x<nw;++x) {
            Channels c={{0}};
            for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
                Channels a=slot->samples[previous+((y*2+dy)%hg)*w+(x*2+dx)%w];
                for(int k=0;k<8;++k) c.v[k]+=a.v[k]*.25f;
            }
            slot->samples[offset+y*nw+x]=c;
        }
        previous=offset;offset+=nw*nh;w=nw;hg=nh;
    }
    slot->valid=true;++builds;return slot;
}
static bool coordinates(const SceneEditorSurfaceMappingCache* c,Vec3 world,Vec3 rest,CoreAuthoredSurfaceCoordinates* out) {
    if(c->map.space==CORE_AUTHORED_SURFACE_OBJECT_REST) {
        double p[]={rest.x,rest.y,rest.z};return core_authored_surface_coordinates(&c->map,p,out);
    }
    HitInfo3D hit;HitInfo3D_Reset(&hit);hit.sceneObjectIndex=c->index;hit.position=world;
    return RuntimeSurfaceMappingCoordinates(&hit,out);
}
static Channels sample(const SceneEditorSurfaceMappingCache* c,double u,double v,int level) {
    int offset=0,w=WIDTH,h=HEIGHT;
    for(int i=0;i<level;++i) {offset+=w*h;w=w>1?w/2:1;h=h>1?h/2:1;}
    double gx=u*w-.5,gy=v*h-.5;int x=(int)floor(gx),y=(int)floor(gy);double fx=gx-floor(gx),fy=gy-floor(gy);
    Channels result={{0}};
    for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
        int ix=((x+dx)%w+w)%w,iy=y+dy;iy=iy<0?0:iy>=h?h-1:iy;
        Channels a=c->samples[offset+iy*w+ix];double weight=(dx?fx:1-fx)*(dy?fy:1-fy);
        for(int k=0;k<8;++k) result.v[k]+=(float)(a.v[k]*weight);
    }
    return result;
}
bool SceneEditorSurfaceMappingCacheSample(const SceneEditorSurfaceMappingCache* c,
    Vec3 world,Vec3 rest,Vec3 world_dx,Vec3 rest_dx,Vec3 world_dy,Vec3 rest_dy,RuntimeMaterialSurfaceEval* out) {
    CoreAuthoredSurfaceCoordinates q;
    if(!c || !c->valid || !out || !coordinates(c,world,rest,&q)) return false;
    double u=q.uv_tiles[0]/c->repeats,v=(q.uv_tiles[1]-c->vmin)/c->vspan;
    if(v<0 || v>1) {
        if(!RuntimeSurfaceMappingEvaluateTiles(c->index,q.uv_tiles[0],q.uv_tiles[1],&c->base,out)) return false;
    } else {
        bool local=c->map.space==CORE_AUTHORED_SURFACE_OBJECT_REST;
        Vec3 p=local?rest:vec3_scale(world,1/c->world_scale);
        Vec3 dx=local?vec3_sub(rest_dx,rest):vec3_scale(vec3_sub(world_dx,world),1/c->world_scale);
        Vec3 dy=local?vec3_sub(rest_dy,rest):vec3_scale(vec3_sub(world_dy,world),1/c->world_scale);
        p=vec3_sub(p,vec3(c->map.origin_m[0],c->map.origin_m[1],c->map.origin_m[2]));
        Vec3 radial=vec3(c->map.axis_u[0],c->map.axis_u[1],c->map.axis_u[2]);
        Vec3 axis=vec3(c->map.axis_v[0],c->map.axis_v[1],c->map.axis_v[2]);
        double x=vec3_dot(p,radial),y=vec3_dot(p,c->tangent),den=fmax(x*x+y*y,1e-20)*6.2831853071795864769;
        /* Analytic angle derivatives stay continuous across the wrap seam. */
        double ux=(x*vec3_dot(dx,c->tangent)-y*vec3_dot(dx,radial))/den;
        double uy=(x*vec3_dot(dy,c->tangent)-y*vec3_dot(dy,radial))/den;
        double vx=vec3_dot(dx,axis)/c->map.tile_m[1]/c->vspan;
        double vy=vec3_dot(dy,axis)/c->map.tile_m[1]/c->vspan;
        double footprint=fmax(1,fmax(hypot(ux*WIDTH,vx*HEIGHT),hypot(uy*WIDTH,vy*HEIGHT)));
        double lod=fmin(LEVELS-1,log2(footprint));int lo=(int)floor(lod),hi=lo<LEVELS-1?lo+1:lo;
        Channels a=sample(c,u,v,lo);
        if(lod>lo) {Channels b=sample(c,u,v,hi);for(int k=0;k<8;++k) a.v[k]+=(b.v[k]-a.v[k])*(lod-lo);}
        *out=eval(a);
    }
    RuntimeSurfaceMappingBlendPole(&c->base,q.source_weight,out);return true;
}
unsigned long long SceneEditorSurfaceMappingCacheBuildCount(void) {return builds;}
