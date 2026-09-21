#include "render/runtime_surface_mapping.h"
#include "render/runtime_material_payload_3d.h"
#include "import/runtime_scene_bridge.h"
#include <math.h>
#include <string.h>

static bool primitive(int index,RuntimeSceneBridgePrimitiveSeed* out,int* ordinal) {
    static RuntimeSceneBridge3DPrimitiveSeedState seeds;
    static unsigned long long cached_revision=~0ull;
    if(cached_revision!=RuntimeSurfaceMappingRevision()) {
        runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
        cached_revision=RuntimeSurfaceMappingRevision();
    }
    for(int i=0;i<seeds.primitive_count;++i) if(seeds.primitives[i].scene_object_index==index) {
        *out=seeds.primitives[i]; *ordinal=i;
        return out->kind==RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE ||
               out->kind==RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
               out->kind==RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX;
    }
    return false;
}
static Vec3 from_local(const RuntimeSceneBridgePrimitiveSeed* p,double x,double y,double z) {
    return vec3(p->origin_x+x*p->axis_u_x+y*p->axis_v_x+z*p->normal_x,
                p->origin_y+x*p->axis_u_y+y*p->axis_v_y+z*p->normal_y,
                p->origin_z+x*p->axis_u_z+y*p->axis_v_z+z*p->normal_z);
}
bool RuntimeSurfaceMaterialPrimitiveIsland(int index,Vec3 point,Vec3 normal,int* face,double* u,double* v) {
    RuntimeSceneBridgePrimitiveSeed p; int ordinal;
    if(!face || !u || !v || !primitive(index,&p,&ordinal)) return false;
    return RuntimeSurfaceMaterialPrimitiveIslandForSeed(&p,point,normal,face,u,v);
}
bool RuntimeSurfaceMaterialPrimitiveIslandForSeed(const RuntimeSceneBridgePrimitiveSeed* seed,
    Vec3 point,Vec3 normal,int* face,double* u,double* v) {
    if(!seed || !face || !u || !v) return false;
    RuntimeSceneBridgePrimitiveSeed p=*seed;
    Vec3 d=vec3_sub(point,vec3(p.origin_x,p.origin_y,p.origin_z));
    Vec3 a=vec3(p.axis_u_x,p.axis_u_y,p.axis_u_z),b=vec3(p.axis_v_x,p.axis_v_y,p.axis_v_z),n=vec3(p.normal_x,p.normal_y,p.normal_z);
    double x=vec3_dot(d,a)/p.width+.5,y=vec3_dot(d,b)/p.height+.5;
    double z=p.depth>1e-12?vec3_dot(d,n)/p.depth+.5:0;
    double nx=vec3_dot(normal,a),ny=vec3_dot(normal,b),nz=vec3_dot(normal,n);
    *face=0;
    if(p.kind!=RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        if(fabs(nz)>=fabs(nx) && fabs(nz)>=fabs(ny)) *face=nz>=0?0:1;
        else if(fabs(ny)>=fabs(nx)) *face=ny>=0?3:2;
        else *face=nx>=0?5:4;
    }
    switch(*face) {
        case 0:*u=x;*v=y;break;case 1:*u=y;*v=x;break;
        case 2:*u=x;*v=z;break;case 3:*u=z;*v=x;break;
        case 4:*u=z;*v=y;break;default:*u=y;*v=z;break;
    }
    return isfinite(*u) && isfinite(*v);
}
bool RuntimeSurfaceMaterialSamplePrimitive(int index,int face,double u,double v,RuntimeMaterialSurfaceEval* out) {
    RuntimeSceneBridgePrimitiveSeed p; int ordinal;
    if(!out || face<0 || face>5 || !isfinite(u) || !isfinite(v) || !primitive(index,&p,&ordinal)) return false;
    double x=0,y=0,z=0; Vec3 normal=vec3(p.normal_x,p.normal_y,p.normal_z);
    switch(face) {
        case 0:x=u-.5;y=v-.5;z=.5;break;
        case 1:x=v-.5;y=u-.5;z=-.5;normal=vec3_scale(normal,-1);break;
        case 2:x=u-.5;y=-.5;z=v-.5;normal=vec3(-p.axis_v_x,-p.axis_v_y,-p.axis_v_z);break;
        case 3:x=v-.5;y=.5;z=u-.5;normal=vec3(p.axis_v_x,p.axis_v_y,p.axis_v_z);break;
        case 4:x=-.5;y=v-.5;z=u-.5;normal=vec3(-p.axis_u_x,-p.axis_u_y,-p.axis_u_z);break;
        case 5:x=.5;y=u-.5;z=v-.5;normal=vec3(p.axis_u_x,p.axis_u_y,p.axis_u_z);break;
    }
    if(p.kind==RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {if(face!=0) return false;z=0;}
    HitInfo3D hit; HitInfo3D_Reset(&hit);
    hit.position=from_local(&p,x*p.width,y*p.height,z*p.depth);
    hit.normal=hit.geometricNormal=hit.shadingNormal=normal;
    hit.sceneObjectIndex=index;hit.primitiveIndex=ordinal;
    hit.localTriangleIndex=face*2+(u<v);
    hit.triangleIndex=hit.localTriangleIndex;
    if(u>=v) {hit.baryU=1-u;hit.baryV=u-v;hit.baryW=v;}
    else {hit.baryU=1-v;hit.baryV=u;hit.baryW=v-u;}
    RuntimeMaterialPayload3D payload;
    if(!RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload) || !payload.valid) return false;
    *out=RuntimeMaterialSurfaceEvalMakeBase(payload.baseColorR,payload.baseColorG,payload.baseColorB,
        payload.bsdf.roughness,payload.bsdf.reflectivity,payload.bsdf.specWeight,payload.bsdf.diffuseWeight,payload.transparency);
    out->textureU=payload.textureU;out->textureV=payload.textureV;out->textureMask=payload.textureMask;
    return true;
}
