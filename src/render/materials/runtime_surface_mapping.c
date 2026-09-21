#include "render/runtime_surface_mapping.h"
#include "import/runtime_scene_bridge.h"
#include "editor/scene_editor_material_stack.h"
#include "editor/scene_editor_material_face_placement.h"
#include "config/config_manager.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct Binding {
    bool active;
    CoreAuthoredSurfaceMapping map;
    RuntimeSceneBridgePrimitiveSeed primitive;
    double source_size[3], world_scale;
} Binding;
static Binding bindings[MAX_OBJECTS];
static unsigned long long revision;

static json_object* field(json_object* o,const char* key) {
    json_object* v=NULL; if(o) json_object_object_get_ex(o,key,&v); return v;
}
static bool token(json_object* o,const char* k,const char* value) {
    json_object* v=field(o,k);
    return v && json_object_is_type(v,json_type_string) && !strcmp(json_object_get_string(v),value);
}
static json_object* mapping(json_object* object) {
    return field(field(field(object,"extensions"),"ray_tracing"),"surface_mapping");
}
static bool number(json_object* v,double* out) {
    if(!v || (!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int))) return false;
    *out=json_object_get_double(v); return isfinite(*out);
}
static bool vector(json_object* o,const char* key,double* out,int count) {
    json_object* v=field(o,key);
    if(!v || !json_object_is_type(v,json_type_array) || json_object_array_length(v)!=(size_t)count) return false;
    for(int i=0;i<count;++i) if(!number(json_object_array_get_idx(v,i),&out[i])) return false;
    return true;
}
static bool parse(json_object* o,CoreAuthoredSurfaceMapping* m) {
    json_object* seed=field(o,"seed"); memset(m,0,sizeof(*m));
    if(!token(o,"required_capability","optic.planar_surface_v1") ||
       !token(o,"method","planar") || !token(o,"source_domain","brick_cells_v1") ||
       !token(o,"scale_policy","stretch_with_object") ||
       !json_object_is_type(field(o,"version"),json_type_int) || json_object_get_int(field(o,"version"))!=1 ||
       !seed || !json_object_is_type(seed,json_type_int) || json_object_get_int64(seed)<0 ||
       json_object_get_uint64(seed)>UINT32_MAX) return false;
    m->version=1; m->seed=(uint32_t)json_object_get_uint64(seed);
    m->space=token(o,"space","object_rest") ? CORE_AUTHORED_SURFACE_OBJECT_REST :
             token(o,"space","world") ? CORE_AUTHORED_SURFACE_WORLD : 0;
    return vector(o,"origin_m",m->origin_m,3) && vector(o,"axis_u",m->axis_u,3) &&
        vector(o,"axis_v",m->axis_v,3) && vector(o,"tile_m",m->tile_m,2) &&
        vector(o,"offset_m",m->offset_m,2) && vector(o,"pivot_m",m->pivot_m,2) &&
        number(field(o,"rotation_rad"),&m->rotation_rad) && core_authored_surface_mapping_validate(m);
}
static bool dimensions(json_object* object,double size[3]) {
    json_object* p=field(object,"primitive");
    size[2]=1;
    return number(field(p,"width"),&size[0]) && size[0]>=0.1 &&
           number(field(p,"height"),&size[1]) && size[1]>=0.1 &&
           (token(object,"object_type","plane_primitive") ||
            (number(field(p,"depth"),&size[2]) && size[2]>=0.1));
}
/* Primitive builders normalize frame vectors. Require an orthonormal source
 * basis so the inverse projection has exactly the same physical meaning. */
static bool frame_supported(json_object* object) {
    json_object* frame=field(field(object,"primitive"),"frame");
    const char* names[]={"axis_u","axis_v","normal"};
    double basis[3][3]={{1,0,0},{0,1,0},{0,0,1}};
    for(int a=0;a<3;++a) {
        json_object* v=field(frame,names[a]);
        if(!v) continue;
        for(int k=0;k<3;++k) {
            char key[]={"xyz"[k],0};
            if(!number(field(v,key),&basis[a][k])) return false;
        }
    }
    for(int a=0;a<3;++a) for(int b=a;b<3;++b) {
        double dot=0; for(int k=0;k<3;++k) dot+=basis[a][k]*basis[b][k];
        if(fabs(dot-(a==b?1.0:0.0))>1e-9) return false;
    }
    double handedness=
        (basis[0][1]*basis[1][2]-basis[0][2]*basis[1][1])*basis[2][0]+
        (basis[0][2]*basis[1][0]-basis[0][0]*basis[1][2])*basis[2][1]+
        (basis[0][0]*basis[1][1]-basis[0][1]*basis[1][0])*basis[2][2];
    return handedness>0;
}
static bool layers_supported(json_object* root,const char* id) {
    json_object* rows=field(field(field(field(root,"extensions"),"ray_tracing"),"authoring"),"object_materials");
    if(!rows || !json_object_is_type(rows,json_type_array)) return false;
    for(size_t i=0;i<json_object_array_length(rows);++i) {
        json_object* row=json_object_array_get_idx(rows,i);
        if(!token(row,"object_id",id)) continue;
        for(size_t k=i+1;k<json_object_array_length(rows);++k)
            if(token(json_object_array_get_idx(rows,k),"object_id",id)) return false;
        /* M1 does not reinterpret graph/image/face source documents. */
        json_object* placements=field(field(row,"procedural_texture"),"face_placements");
        if(field(row,"material_graph") || field(row,"materialGraph") || field(row,"authored_texture") ||
           (placements && (!json_object_is_type(placements,json_type_array) || json_object_array_length(placements)>0))) return false;
        json_object* layers=field(field(row,"material_texture_stack"),"layers");
        if(!layers || !json_object_is_type(layers,json_type_array) ||
           json_object_array_length(layers)==0 || json_object_array_length(layers)>8) return false;
        for(size_t j=0;j<json_object_array_length(layers);++j) {
            json_object* layer=json_object_array_get_idx(layers,j);
            if(!token(layer,"kind","brick") && !token(layer,"kind","solid")) return false;
            if(!field(layer,"id") || !json_object_is_type(field(layer,"id"),json_type_string) ||
               !json_object_get_string(field(layer,"id"))[0] ||
               strlen(json_object_get_string(field(layer,"id")))>=RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE) return false;
            for(size_t k=0;k<j;++k)
                if(token(json_object_array_get_idx(layers,k),"id",json_object_get_string(field(layer,"id")))) return false;
        }
        return true;
    }
    return false;
}
bool RuntimeSurfaceMappingValidateScene(json_object* root,char* diagnostic,size_t size) {
    json_object* objects=field(root,"objects");
    for(size_t i=0;json_object_is_type(objects,json_type_array) && i<json_object_array_length(objects);++i) {
        json_object* object=json_object_array_get_idx(objects,i); json_object* m=NULL;
        json_object* ray=field(field(object,"extensions"),"ray_tracing");
        if(!ray || !json_object_object_get_ex(ray,"surface_mapping",&m)) continue;
        CoreAuthoredSurfaceMapping parsed; double dims[3]={0};
        bool valid=(token(object,"object_type","plane_primitive") || token(object,"object_type","rect_prism_primitive")) &&
            field(object,"object_id") && json_object_is_type(field(object,"object_id"),json_type_string) &&
            parse(m,&parsed) && dimensions(object,dims) && frame_supported(object) &&
            layers_supported(root,json_object_get_string(field(object,"object_id")));
        double world_scale=0;
        if(!number(field(root,"world_scale"),&world_scale) || world_scale<=0) valid=false;
        json_object* scale=field(field(object,"transform"),"scale");
        for(int a=0;a<3;++a) {
            double value=1; char key[2]={"xyz"[a],0};
            if(field(scale,key) && (!number(field(scale,key),&value) || value<=0)) valid=false;
            double size=dims[a]*value*world_scale;
            if((a<2 || token(object,"object_type","rect_prism_primitive")) &&
               (!isfinite(size) || size<0.1)) valid=false;
        }
        if(!valid) {
            snprintf(diagnostic,size,"surface_mapping object %zu: unsupported/invalid planar v1 contract, geometry, scale or source (M1 requires brick/solid stack)",i);
            return false;
        }
    }
    return true;
}
void RuntimeSurfaceMappingLoadScene(json_object* root,double world_scale) {
    memset(bindings,0,sizeof(bindings)); ++revision;
    RuntimeSceneBridge3DPrimitiveSeedState seeds={0};
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    json_object* objects=field(root,"objects");
    for(size_t i=0;json_object_is_type(objects,json_type_array) && i<json_object_array_length(objects);++i) {
        json_object* object=json_object_array_get_idx(objects,i); json_object* m=mapping(object);
        if(!m) continue;
        for(int j=0;j<seeds.primitive_count;++j) {
            const RuntimeSceneBridgePrimitiveSeed* p=&seeds.primitives[j];
            if(!token(object,"object_id",p->object_id) || p->scene_object_index<0 || p->scene_object_index>=MAX_OBJECTS) continue;
            Binding* b=&bindings[p->scene_object_index];
            b->active=parse(m,&b->map) && dimensions(object,b->source_size);
            b->primitive=*p; b->world_scale=world_scale;
        }
    }
}
bool RuntimeSurfaceMappingActive(int i) { return animSettings.sceneSource==SCENE_SOURCE_RUNTIME_SCENE && i>=0 && i<MAX_OBJECTS && bindings[i].active; }
unsigned long long RuntimeSurfaceMappingRevision(void) {return revision;}
bool RuntimeSurfaceMappingCoordinates(const HitInfo3D* hit,CoreAuthoredSurfaceCoordinates* out) {
    if(!hit || !out || !RuntimeSurfaceMappingActive(hit->sceneObjectIndex)) return false;
    const Binding* b=&bindings[hit->sceneObjectIndex]; const CoreAuthoredSurfaceMapping* m=&b->map;
    const RuntimeSceneBridgePrimitiveSeed* p=&b->primitive;
    double point[3]={hit->position.x/b->world_scale,hit->position.y/b->world_scale,hit->position.z/b->world_scale};
    if(m->space==CORE_AUTHORED_SURFACE_OBJECT_REST) {
        Vec3 d=vec3_sub(hit->position,vec3(p->origin_x,p->origin_y,p->origin_z));
        point[0]=(d.x*p->axis_u_x+d.y*p->axis_u_y+d.z*p->axis_u_z)*b->source_size[0]/p->width;
        point[1]=(d.x*p->axis_v_x+d.y*p->axis_v_y+d.z*p->axis_v_z)*b->source_size[1]/p->height;
        point[2]=p->depth>1e-12 ? (d.x*p->normal_x+d.y*p->normal_y+d.z*p->normal_z)*b->source_size[2]/p->depth : 0;
    }
    double u=0,v=0;
    for(int i=0;i<3;++i) {u+=(point[i]-m->origin_m[i])*m->axis_u[i];v+=(point[i]-m->origin_m[i])*m->axis_v[i];}
    u-=m->pivot_m[0];v-=m->pivot_m[1];
    const double c=cos(m->rotation_rad),s=sin(m->rotation_rad);
    memset(out,0,sizeof(*out));
    out->uv_tiles[0]=(c*u-s*v+m->pivot_m[0]+m->offset_m[0])/m->tile_m[0];
    out->uv_tiles[1]=(s*u+c*v+m->pivot_m[1]+m->offset_m[1])/m->tile_m[1];
    out->valid=isfinite(out->uv_tiles[0]) && isfinite(out->uv_tiles[1]) &&
        fabs(out->uv_tiles[0])<1e6 && fabs(out->uv_tiles[1])<1e6;
    return out->valid;
}
bool RuntimeSurfaceMappingEvaluate(const SceneObject* object,const HitInfo3D* hit,
    const RuntimeMaterialSurfaceEval* base,RuntimeMaterialSurfaceEval* out) {
    CoreAuthoredSurfaceCoordinates coordinates; RuntimeMaterialTextureStack stack;
    if(!RuntimeSurfaceMappingCoordinates(hit,&coordinates) ||
       !SceneEditorMaterialStackGetEffectiveObjectStack(object,hit->sceneObjectIndex,&stack)) return false;
    return RuntimeMaterialTextureStackEvaluateBrickCells(&stack,coordinates.uv_tiles[0],coordinates.uv_tiles[1],
        bindings[hit->sceneObjectIndex].map.seed,base,out);
}
