#include "editor/scene_editor_viewport_material.h"
#include "editor/material_preview_surface_eval.h"
#include "render/runtime_surface_mapping.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_material_face_placement.h"
#include "config/config_manager.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define GRID 128
#define CACHE_SLOTS 32
/* Fixed storage: ~16 MiB, no allocations or texture-stack evaluation during orbit
 * while the visible working set fits the 32 material/face slots. */
typedef struct Sample { float r,g,b,rough,reflection,specular,diffuse,transparency; } Sample;
struct SceneEditorViewportMaterial {
    bool valid;
    int object_index,face;
    uint64_t hash,stamp;
    double emission;
    Sample samples[GRID*GRID];
};
static SceneEditorViewportMaterial cache[CACHE_SLOTS];
static uint64_t clock_tick,builds;
static double clamp01(double x) {return fmax(0.0,fmin(1.0,x));}
static uint64_t hash_bytes(uint64_t hash,const void* data,size_t size) {
    const unsigned char* bytes=data;for(size_t i=0;i<size;++i) {hash^=bytes[i];hash*=UINT64_C(1099511628211);}return hash;
}
static const SceneEditorViewportMaterial* prepare(int index,int face) {
    if(index<0 || index>=sceneSettings.objectCount) return NULL;
    const SceneObject* object=&sceneSettings.sceneObjects[index];
    RuntimeMaterialTextureStack stack={0};RuntimeMaterialSurfaceEval base={0};
    if(!MaterialPreviewSurfacePrepareObject(object,index,&stack,&base)) return NULL;
    if(face>=0) SceneEditorMaterialFacePlacementApplyOverridesToStack(object,index,face,&stack);
    int seed=object->textureSeed ? object->textureSeed : index+1;
    uint64_t hash=hash_bytes(UINT64_C(1469598103934665603),&stack,sizeof(stack));
    hash=hash_bytes(hash,&base,sizeof(base));hash=hash_bytes(hash,&seed,sizeof(seed));
    hash=hash_bytes(hash,&object->emissiveStrength,sizeof(object->emissiveStrength));
    unsigned long long revision=RuntimeSurfaceMappingRevision(),document=SceneEditorDocumentRevision();
    hash=hash_bytes(hash,&revision,sizeof(revision));
    hash=hash_bytes(hash,&document,sizeof(document));
    unsigned long long images=RuntimeMaterialAuthoredTextureRevision();
    hash=hash_bytes(hash,&images,sizeof(images));
    SceneEditorViewportMaterial* slot=&cache[0];
    for(int i=0;i<CACHE_SLOTS;++i) {
        if(cache[i].valid && cache[i].object_index==index && cache[i].face==face && cache[i].hash==hash) {cache[i].stamp=++clock_tick;return &cache[i];}
        if(cache[i].stamp<slot->stamp) slot=&cache[i];
    }
    slot->valid=true;slot->hash=hash;slot->object_index=index;slot->face=face;slot->stamp=++clock_tick;
    slot->emission=clamp01(object->emissiveStrength);
    for(int y=0;y<GRID;++y) for(int x=0;x<GRID;++x) {
        RuntimeMaterialSurfaceEval eval=base;
        if(face>=0) {
            if(!RuntimeSurfaceMaterialSamplePrimitive(index,face,(double)x/(GRID-1),(double)y/(GRID-1),&eval)) {slot->valid=false;return NULL;}
        } else RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,object,(double)x/(GRID-1),(double)y/(GRID-1),seed,&base,&eval);
        slot->samples[y*GRID+x]=(Sample){eval.colorR,eval.colorG,eval.colorB,eval.roughness,eval.reflectivity,eval.specWeight,eval.diffuseWeight,eval.transparency};
    }
    ++builds;return slot;
}
const SceneEditorViewportMaterial* SceneEditorViewportMaterialPrepare(int index) {return prepare(index,-1);}
const SceneEditorViewportMaterial* SceneEditorViewportMaterialPrepareFace(int index,int face) {return prepare(index,face);}

bool SceneEditorViewportMaterialSample(const SceneEditorViewportMaterial* material,double u,double v,RuntimeMaterialSurfaceEval* out) {
    if(!material || !material->valid || !out || !isfinite(u) || !isfinite(v)) return false;
    double gx=clamp01(u)*(GRID-1),gy=clamp01(v)*(GRID-1);int x=(int)gx,y=(int)gy;
    int x1=x<GRID-1?x+1:x,y1=y<GRID-1?y+1:y;double fx=gx-x,fy=gy-y;
    Sample a=material->samples[y*GRID+x],b=material->samples[y*GRID+x1],c=material->samples[y1*GRID+x],d=material->samples[y1*GRID+x1],s;
#define MIX(field) s.field=(a.field*(1-fx)+b.field*fx)*(1-fy)+(c.field*(1-fx)+d.field*fx)*fy
    MIX(r);MIX(g);MIX(b);MIX(rough);MIX(reflection);MIX(specular);MIX(diffuse);MIX(transparency);
#undef MIX
    *out=RuntimeMaterialSurfaceEvalMakeBase(s.r,s.g,s.b,s.rough,s.reflection,s.specular,s.diffuse,s.transparency);
    return true;
}
static SceneEditorMeshPreviewShadeNormal normalized(SceneEditorMeshPreviewShadeNormal n) {
    double length=sqrt(n.x*n.x+n.y*n.y+n.z*n.z);if(length<1e-12) return (SceneEditorMeshPreviewShadeNormal){0,0,1};
    return (SceneEditorMeshPreviewShadeNormal){n.x/length,n.y/length,n.z/length};
}
static double dot(SceneEditorMeshPreviewShadeNormal a,SceneEditorMeshPreviewShadeNormal b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
static double smooth_edge(double distance,double softness) {double t=clamp01(0.5-distance/softness);return t*t*(3-2*t);}
SDL_Color SceneEditorViewportMaterialShade(const SceneEditorViewportMaterial* material,
    SceneEditorMeshPreviewShadeNormal n,SceneEditorMeshPreviewShadeNormal view,double u,double v) {
    if(!material) return SceneEditorMeshPreviewShadeColor((SDL_Color){160,160,160,255},n);
    RuntimeMaterialSurfaceEval eval;
    if(!SceneEditorViewportMaterialSample(material,u,v,&eval)) return (SDL_Color){255,0,255,255};
    Sample s={eval.colorR,eval.colorG,eval.colorB,eval.roughness,eval.reflectivity,eval.specWeight,eval.diffuseWeight,eval.transparency};
    n=normalized(n);double nv=dot(n,view);if(nv<0) {n.x=-n.x;n.y=-n.y;n.z=-n.z;nv=-nv;}
    SceneEditorMeshPreviewShadeNormal light={-0.365148, -0.182574, 0.912871};
    SceneEditorMeshPreviewShadeNormal half=normalized((SceneEditorMeshPreviewShadeNormal){light.x+view.x,light.y+view.y,light.z+view.z});
    double nl=fmax(0,dot(n,light)),nh=fmax(0,dot(n,half)),rough=clamp01(s.rough);
    double spec=pow(nh,2.0+126.0*(1-rough)*(1-rough))*clamp01(s.specular)*(1-0.65*rough);
    SceneEditorMeshPreviewShadeNormal reflect={2*nv*n.x-view.x,2*nv*n.y-view.y,2*nv*n.z-view.z};
    /* Analytic fixed studio: horizon and two softboxes. Roughness broadens edges.
     * This is inspection lighting, not scene-object reflection or path tracing. */
    double softness=0.02+rough*0.8;
    double box=smooth_edge(fabs(reflect.x+0.38)-0.16,softness)*smooth_edge(fabs(reflect.z-0.5)-0.42,softness);
    double strip=smooth_edge(fabs(reflect.x-0.6)-0.07,softness)*smooth_edge(fabs(reflect.z-0.3)-0.6,softness);
    double env=0.07+0.24*clamp01(reflect.z*0.5+0.5)+(box*0.9+strip*0.6)*(1-0.5*rough);
    double fresnel=pow(1-clamp01(nv),5),reflection=clamp01(s.reflection);
    double f=reflection+(1-reflection)*fresnel*clamp01(s.specular);
    double diffuse=(0.18+0.82*nl)*clamp01(s.diffuse)*(1-reflection);
    double rgb[3]={s.r,s.g,s.b};Uint8 output[3];
    for(int i=0;i<3;++i) {
        double value=rgb[i]*diffuse+env*f*(0.4+0.6*rgb[i])+spec+rgb[i]*material->emission;
        output[i]=(Uint8)lround(255*clamp01(value));
    }
    return (SDL_Color){output[0],output[1],output[2],255};
}
void SceneEditorViewportMaterialReset(void) {memset(cache,0,sizeof(cache));clock_tick=builds=0;}
unsigned long long SceneEditorViewportMaterialBuildCount(void) {return builds;}
