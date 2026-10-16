#include "render/runtime_transport_weight_probe_3d.h"
#include "render/runtime_native_3d_pixel_probe.h"
#include "render/runtime_specular_bsdf_3d.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_path_termination_probe_3d.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static bool enabled;
static _Thread_local struct { bool active; int x,y,events; unsigned sequence,subpass; } context;
static void init(void) {
    const char* env=getenv("RAY_TRACING_TRANSPORT_WEIGHT_PROBE");
    enabled=env && strcmp(env,"1")==0;
}
void RuntimeTransportWeightProbe3D_Begin(int x,int y,const RuntimeNative3DSamplingContext* s) {
    RuntimePathTermination3D_Begin(x,y,s);
    pthread_once(&once,init);
    context.active=enabled && RuntimeNative3DPixelProbe_IsSelected(x,y);
    context.x=x;context.y=y;context.events=0;
    context.sequence=s?s->sampleSequence:0;context.subpass=s?s->temporalSubpassIndex:0;
}
void RuntimeTransportWeightProbe3D_End(void) { context.active=false; RuntimePathTermination3D_End(); }
static bool begin(const char* stage,int depth) {
    if(!context.active || context.events++>=128) return false;
    pthread_mutex_lock(&lock);
    flockfile(stderr);
    fprintf(stderr,"TRANSPORT_WEIGHT {\"stage\":\"%s\",\"x\":%d,\"y\":%d,\"sequence\":%u,\"subpass\":%u,\"depth\":%d",
        stage,context.x,context.y,context.sequence,context.subpass,depth);
    return true;
}
static void vector(const char* name,Vec3 v) {
    fprintf(stderr,",\"%s\":[%.17g,%.17g,%.17g]",name,v.x,v.y,v.z);
}
static void end(void) { fputs("}\n",stderr);funlockfile(stderr);pthread_mutex_unlock(&lock); }
void RuntimeTransportWeightProbe3D_Sample(const HitInfo3D* h,const RuntimeMaterialPayload3D* p,
    const RuntimePrincipledBSDF3D* b,Vec3 incoming,int depth,double pd,double ps,double pt,
    const RuntimeDisneyV2Transport3DVertexSample* s) {
    if(!h || !b || !s || !begin("sample",depth)) return;
    Vec3 view=vec3_scale(vec3_normalize(incoming),-1), n=HitInfo3D_ShadingNormalForReflection(h,view);
    Vec3 half=vec3_normalize(vec3_add(view,s->direction));
    double vh=fmax(0,vec3_dot(view,half));
    RuntimeDielectricTransport3D d={0};
    bool resolved=s->dielectricResolved;
    if(resolved) d=s->dielectric;
    else resolved=p && RuntimeCameraDielectric3D_Resolve(NULL,p,h,incoming,&d);
    fprintf(stderr,",\"object\":%d,\"triangle\":%d,\"lobe\":%d,\"probabilities\":[%.17g,%.17g,%.17g],\"pdf\":%.17g,\"conditional_reflection_pdf\":%.17g,\"cos_view\":%.17g,\"cos_out\":%.17g,\"view_half_cos\":%.17g,\"roughness\":%.17g,\"material_weights\":[%.17g,%.17g,%.17g],\"f0\":[%.17g,%.17g,%.17g],\"reflection_fresnel\":[%.17g,%.17g,%.17g],\"interface_resolved\":%s,\"eta_from\":%.17g,\"eta_to\":%.17g,\"interface_fresnel\":%.17g,\"tir\":%s,\"thin_walled\":%s",
        h->sceneObjectIndex,h->triangleIndex,s->lobe,pd,ps,pt,s->pdf,
        RuntimeSpecularBSDF3D_Pdf(b,h,view,s->direction),vec3_dot(view,n),s->cosTheta,vh,b->roughness,
        b->diffuseWeight,b->specularWeight,b->transmissionWeight,b->specularF0R,b->specularF0G,b->specularF0B,
        RuntimePrincipledBSDF3D_FresnelSchlick(vh,b->specularF0R),RuntimePrincipledBSDF3D_FresnelSchlick(vh,b->specularF0G),RuntimePrincipledBSDF3D_FresnelSchlick(vh,b->specularF0B),
        resolved?"true":"false",d.etaFrom,d.etaTo,d.fresnel,d.totalInternalReflection?"true":"false",p&&p->thinWalled?"true":"false");
    vector("position",h->position);vector("normal",h->normal);vector("incoming",incoming);
    vector("direction",s->direction);vector("factor",vec3(s->throughputR,s->throughputG,s->throughputB));end();
}
void RuntimeTransportWeightProbe3D_Weight(int depth,Vec3 parent,Vec3 factor,double survival,double u,double share) {
    if (survival < 1)
        RuntimePathTermination3D_Record(u > survival ? RUNTIME_PATH_EXIT_ROULETTE_TERMINATED
                                                    : RUNTIME_PATH_EXIT_ROULETTE_SURVIVED, depth,
                                       vec3(parent.x*factor.x,parent.y*factor.y,parent.z*factor.z));
    if(!begin("weight",depth)) return;
    vector("parent",parent);vector("factor",factor);
    vector("before_roulette",vec3(parent.x*factor.x,parent.y*factor.y,parent.z*factor.z));
    fprintf(stderr,",\"survival\":%.17g,\"roulette_sample\":%.17g,\"roulette_terminated\":%s,\"local_share\":%.17g",survival,u,u>survival?"true":"false",share);end();
}
void RuntimeTransportWeightProbe3D_Direct(int depth,const HitInfo3D* hit,Vec3 area,Vec3 radiance,double mis) {
    if(!begin("direct",depth)) return;
    fprintf(stderr,",\"object\":%d,\"triangle\":%d,\"mis_weight\":%.17g",hit->sceneObjectIndex,hit->triangleIndex,mis);
    vector("area_weight",area);vector("radiance",radiance);
    vector("contribution",vec3(area.x*radiance.x*mis,area.y*radiance.y*mis,area.z*radiance.z*mis));end();
}
