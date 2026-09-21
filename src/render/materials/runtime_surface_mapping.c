#include "render/runtime_surface_mapping.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_mesh_asset_loader.h"
#include "editor/scene_editor_material_stack.h"
#include "editor/scene_editor_material_face_placement.h"
#include "render/runtime_material_graph_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "app/ray_tracing_sha256.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "config/config_manager.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct LayerMappings {
    bool active[8];
    CoreAuthoredSurfaceMapping maps[8];
} LayerMappings;

typedef struct RegionBinding {
    bool active, has_map, has_stack;
    CoreAuthoredSurfaceMapping map;
    RuntimeMaterialTextureStack stack;
    LayerMappings layers;
} RegionBinding;

typedef struct Binding {
    bool active, mesh, authored, asset_graph;
    RegionBinding regions[6];
    LayerMappings layers;
    int mapping_count;
    char mapping_ids[16][64];
    CoreAuthoredSurfaceMapping mappings[16];
    double position[3],scale[3],basis[3][3];
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
    if(token(o,"method","authored_uv")) {
        const char *id=json_object_get_string(field(o,"uv_set_id"));
        m->version=3;
        if(!token(o,"required_capability","optic.authored_uv_v1") || !token(o,"source_domain","brick_cells_v1") ||
           !json_object_is_type(field(o,"version"),json_type_int) || json_object_get_int(field(o,"version"))!=3 ||
           !json_object_is_type(field(o,"uv_set_id"),json_type_string) || !id || !id[0] || strlen(id)>=sizeof(m->uv_set_id) || !json_object_is_type(seed,json_type_int) ||
           json_object_get_int64(seed)<0 || json_object_get_uint64(seed)>UINT32_MAX) return false;
        snprintf(m->uv_set_id,sizeof(m->uv_set_id),"%s",id);m->seed=(uint32_t)json_object_get_uint64(seed);
        return vector(o,"uv_scale",m->uv_scale,2) && vector(o,"uv_offset",m->uv_offset,2) &&
            number(field(o,"rotation_rad"),&m->rotation_rad) && core_authored_surface_mapping_validate(m);
    }
    bool axial=token(o,"method","axial_height");
    int version=axial?2:1;
    if(!token(o,"required_capability",axial?"optic.axial_surface_v2":"optic.planar_surface_v1") ||
       (!axial && !token(o,"method","planar")) || !token(o,"source_domain","brick_cells_v1") ||
       !token(o,"scale_policy","stretch_with_object") ||
       !json_object_is_type(field(o,"version"),json_type_int) || json_object_get_int(field(o,"version"))!=version ||
       !seed || !json_object_is_type(seed,json_type_int) || json_object_get_int64(seed)<0 ||
       json_object_get_uint64(seed)>UINT32_MAX) return false;
    m->version=version; m->seed=(uint32_t)json_object_get_uint64(seed);
    m->space=token(o,"space","object_rest") ? CORE_AUTHORED_SURFACE_OBJECT_REST :
             token(o,"space","world") ? CORE_AUTHORED_SURFACE_WORLD : 0;
    if(axial && (!number(field(o,"reference_radius_m"),&m->reference_radius_m) ||
        !number(field(o,"seam_rad"),&m->seam_rad) || !number(field(o,"pole_radius_m"),&m->pole_radius_m) ||
        !vector(o,"height_range_m",m->height_range_m,2) ||
        !token(o,"pole_policy","fade_to_base") || !token(o,"repeat_policy","integer_circumference"))) return false;
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
/* Validate the original graph before the legacy compiler can normalize, skip,
 * truncate or synthesize identifiers. The retained JSON remains authoring truth. */
static json_object* source_layers(json_object* row) {
    json_object* graph=field(row,"material_graph");
    if(!graph) graph=field(row,"materialGraph");
    if(!graph) return json_object_get(field(field(row,"material_texture_stack"),"layers"));
    if(field(row,"material_graph") && field(row,"materialGraph")) return NULL;
    if(field(row,"material_texture_stack") || field(row,"materialTextureStack")) return NULL;
    json_object* schema=field(graph,"schema_version");if(!schema) schema=field(graph,"schemaVersion");
    json_object* graph_id=field(graph,"graph_id");if(!graph_id) graph_id=field(graph,"graphId");
    if(!json_object_is_type(schema,json_type_int) || json_object_get_int(schema)!=1 ||
       !json_object_is_type(graph_id,json_type_string) || !json_object_get_string(graph_id)[0] ||
       strlen(json_object_get_string(graph_id))>=RUNTIME_MATERIAL_GRAPH_ID_CAPACITY) return NULL;
    json_object* nodes=field(graph,"nodes");
    if(!json_object_is_type(nodes,json_type_array) || !json_object_array_length(nodes) ||
       json_object_array_length(nodes)>8) return NULL;
    json_object* layers=json_object_new_array();
    for(size_t i=0;i<json_object_array_length(nodes);++i) {
        json_object* node=json_object_array_get_idx(nodes,i);
        json_object* id=field(node,"node_id");if(!id) id=field(node,"nodeId");
        if(!json_object_is_type(id,json_type_string) || !json_object_get_string(id)[0] ||
           strlen(json_object_get_string(id))>=RUNTIME_MATERIAL_GRAPH_NODE_ID_CAPACITY ||
           (!token(node,"node_kind","layer") && !token(node,"nodeKind","layer")) ||
           !json_object_is_type(field(node,"layer"),json_type_object)) goto invalid;
        for(size_t j=0;j<i;++j) {
            json_object* other=json_object_array_get_idx(nodes,j);
            if(token(other,"node_id",json_object_get_string(id)) || token(other,"nodeId",json_object_get_string(id))) goto invalid;
        }
        json_object_array_add(layers,json_object_get(field(node,"layer")));
    }
    RuntimeMaterialGraphDocument document;RuntimeMaterialGraphCompileResult compiled;
    if(!RuntimeMaterialGraphDocumentFromJsonObject(graph,&document) ||
       !RuntimeMaterialGraphCompileToStack(&document,&compiled) || compiled.channelRefCount ||
       compiled.stack.layerCount!=(int)json_object_array_length(layers)) goto invalid;
    return layers;
invalid:
    json_object_put(layers);return NULL;
}
static bool source_supported(json_object* row,bool axial) {
        if(!json_object_is_type(row,json_type_object)) return false;
        json_object* layers=source_layers(row);
        if(!layers || !json_object_is_type(layers,json_type_array) ||
           json_object_array_length(layers)==0 || json_object_array_length(layers)>8) {if(layers) json_object_put(layers);return false;}
        bool supported=true;
        for(size_t j=0;j<json_object_array_length(layers);++j) {
            json_object* layer=json_object_array_get_idx(layers,j);
            if(field(layer,"enabled") && !json_object_is_type(field(layer,"enabled"),json_type_boolean)) supported=false;
            const char* property_groups[]={"","placement","parameters"};
            const char* properties[][9]={
                {"opacity","roughness_influence","reflectivity_influence","specular_influence","diffuse_influence","transparency_influence",NULL},
                {"offset_u","offset_v","scale","rotation","strength",NULL},
                {"grain","coverage","contrast","edge_softness","flow","color_depth","surface_damage","seed",NULL}};
            for(int g=0;g<3;++g) {
                json_object* owner=g?field(layer,property_groups[g]):layer;
                if(owner && !json_object_is_type(owner,json_type_object)) supported=false;
                for(int k=0;properties[g][k];++k) {
                    const char* key=properties[g][k];char camel[64];size_t length=0;
                    for(size_t c=0;key[c];++c) {
                        if(key[c]=='_' && key[c+1]) {++c;camel[length++]=(char)(key[c]-'a'+'A');}
                        else camel[length++]=key[c];
                    }
                    camel[length]=0;
                    json_object* values[2]={field(owner,key),strcmp(camel,key)?field(owner,camel):NULL};
                    for(int alias=0;alias<2;++alias) {
                        double n;if(!values[alias]) continue;
                        if(!number(values[alias],&n)) {supported=false;continue;}
                        if(!strcmp(key,"scale")) {if(n<=0 || n>1e6) supported=false;}
                        else if(!strcmp(key,"offset_u") || !strcmp(key,"offset_v") || !strcmp(key,"rotation")) {if(fabs(n)>1e6) supported=false;}
                        else if(!strcmp(key,"seed")) {if(n<0 || n>16777215 || floor(n)!=n) supported=false;}
                        else if(n<(strstr(key,"influence")?-1:0) || n>1) supported=false;
                    }
                }
            }
            if(axial) {
                json_object* placement=field(layer,"placement");
                const char* keys[]={"scale","rotation"};
                const double expected[]={1,0};
                for(int k=0;k<2;++k) {
                    double n=expected[k];
                    if(field(placement,keys[k]) && (!number(field(placement,keys[k]),&n) || n!=expected[k])) supported=false;
                }
            }
            if(!token(layer,"kind","brick") && !token(layer,"kind","solid")) supported=false;
            if(!field(layer,"id") || !json_object_is_type(field(layer,"id"),json_type_string) ||
               !json_object_get_string(field(layer,"id"))[0] ||
               strlen(json_object_get_string(field(layer,"id")))>=RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE) {supported=false;continue;}
            for(size_t k=0;k<j;++k)
                if(token(json_object_array_get_idx(layers,k),"id",json_object_get_string(field(layer,"id")))) supported=false;
        }
        json_object_put(layers);return supported;
}
static bool layers_supported(json_object* root,const char* id,bool axial) {
    json_object* rows=field(field(field(field(root,"extensions"),"ray_tracing"),"authoring"),"object_materials");
    if(!rows || !json_object_is_type(rows,json_type_array)) return false;
    for(size_t i=0;i<json_object_array_length(rows);++i) {
        json_object* row=json_object_array_get_idx(rows,i);
        if(!token(row,"object_id",id)) continue;
        for(size_t k=i+1;k<json_object_array_length(rows);++k)
            if(token(json_object_array_get_idx(rows,k),"object_id",id)) return false;
        /* Graph integration is explicit; M1/M2 documents retain their gate. */
        json_object* binding=field(row,"surface_material_binding");
        bool m3=binding && token(binding,"required_capability","optic.surface_material_v3") &&
            json_object_is_type(field(binding,"version"),json_type_int) && json_object_get_int(field(binding,"version"))==1;
        if(binding && !m3) return false;
        json_object* placements=field(field(row,"procedural_texture"),"face_placements");
        if((!m3 && (field(row,"material_graph") || field(row,"materialGraph") || field(row,"authored_texture"))) ||
           (placements && (!json_object_is_type(placements,json_type_array) || json_object_array_length(placements)>0))) return false;
        return source_supported(row,axial);
    }
    return false;
}
static json_object* material_row(json_object* root,const char* id) {
    json_object* rows=field(field(field(field(root,"extensions"),"ray_tracing"),"authoring"),"object_materials");
    for(size_t i=0;json_object_is_type(rows,json_type_array) && i<json_object_array_length(rows);++i) {
        json_object* row=json_object_array_get_idx(rows,i);
        if(token(row,"object_id",id)) return row;
    }
    return NULL;
}
static int region_face(json_object* region) {
    const char* roles[]={"front","back","left","right","top","bottom"};
    for(int i=0;i<6;++i) if(token(region,"face_role",roles[i])) return i;
    return -1;
}
static json_object* resolve_mapping(json_object* binding,json_object* reference) {
    if(!json_object_is_type(reference,json_type_string)) return NULL;
    const char* id=json_object_get_string(reference);
    json_object* maps=field(binding,"mappings");
    for(size_t i=0;json_object_is_type(maps,json_type_array) && i<json_object_array_length(maps);++i) {
        json_object* entry=json_object_array_get_idx(maps,i);
        if(token(entry,"id",id)) return field(entry,"definition");
    }
    return NULL;
}
static bool layer_maps(json_object* source,json_object* binding,bool mesh,LayerMappings* out) {
    if(out) memset(out,0,sizeof(*out));
    json_object* layers=source_layers(source);if(!layers) return false;
    bool valid=true;
    for(size_t i=0;i<json_object_array_length(layers);++i) {
        json_object* layer=json_object_array_get_idx(layers,i),*ref=field(layer,"mapping_ref");
        if(!ref) continue;
        CoreAuthoredSurfaceMapping map;
        if(mesh || !parse(resolve_mapping(binding,ref),&map)) {valid=false;break;}
        /* A per-layer projection must obey the same source-placement constraints. */
        if(map.version==2) {
            json_object* placement=field(layer,"placement");double scale=1,rotation=0;
            if((field(placement,"scale") && !number(field(placement,"scale"),&scale)) ||
               (field(placement,"rotation") && !number(field(placement,"rotation"),&rotation))) valid=false;
            if(scale!=1 || rotation!=0) valid=false;
        }
        if(out && i<8) {out->active[i]=true;out->maps[i]=map;}
    }
    json_object_put(layers);return valid;
}
/* Resource dependencies are verified once during preparation, never per ray.
 * A source document selects a named mapping; its bytes and mapping bytes are
 * separately pinned so editing either invalidates a stale authoring reference. */
static bool source_documents_supported(json_object* binding,const char* object_id) {
    json_object* docs=field(binding,"source_documents");
    if(!docs) return true;
    if(!json_object_is_type(docs,json_type_array) || json_object_array_length(docs)>16) return false;
    for(size_t i=0;i<json_object_array_length(docs);++i) {
        json_object* item=json_object_array_get_idx(docs,i);
        const char* path=json_object_get_string(field(item,"path"));
        const char* expected=json_object_get_string(field(item,"sha256"));char digest[65];
        if(!path || path[0]!='/' || !ray_tracing_sha256_is_valid_hex(expected) ||
           !ray_tracing_sha256_file(path,digest) || strcmp(digest,expected)) return false;
        json_object* doc=json_object_from_file(path);if(!doc) return false;
        bool valid=false;json_object* ref=NULL;
        if(token(item,"kind","surface_authoring_document") && token(doc,"schema","ray_tracing.surface_authoring_document")) {
            ref=field(field(doc,"surface_mapping"),"id");
            const char* expected_map=json_object_get_string(field(field(doc,"surface_mapping"),"digest_sha256"));
            json_object* maps=field(binding,"mappings");
            for(size_t j=0;ref && maps && j<json_object_array_length(maps);++j) {
                json_object* map=json_object_array_get_idx(maps,j);
                if(token(map,"id",json_object_get_string(ref)) && expected_map && token(map,"sha256",expected_map) && token(doc,"source_object_id",object_id)) valid=true;
            }
        } else if(token(item,"kind","solid_graph") && token(doc,"schema","ray_tracing.procedural_solid_material_composition_graph")) {
            ref=field(doc,"surface_mapping_ref");valid=true;
        } else if(token(item,"kind","authored_manifest") && field(doc,"export_binding_kind")) {
            ref=field(doc,"surface_mapping_ref");valid=token(doc,"source_object_id",object_id);
        } else if(token(item,"kind","layer_graph") && field(doc,"nodes")) {
            ref=field(doc,"surface_mapping_ref");valid=true;
        }
        valid=valid && ref && resolve_mapping(binding,ref);
        json_object_put(doc);if(!valid) return false;
    }
    return true;
}
static bool external_mapping_supported(json_object* row,json_object* object,json_object* binding) {
    if(field(row,"authored_texture") && token(object,"object_type","mesh_asset_instance")) return false;
    json_object* sources[2]={field(row,"authored_texture"),field(object,"procedural_solid_material_ref")};
    const char* keys[2]={"manifest_path","graph_path"};
    for(int i=0;i<2;++i) {
        json_object* value=field(sources[i],keys[i]);if(!value) continue;
        const char* path=json_object_get_string(value);
        if(!path || path[0]!='/') return false;
        json_object* doc=json_object_from_file(path);if(!doc) return false;
        json_object* reference=NULL;
        bool has_reference=json_object_object_get_ex(doc,"surface_mapping_ref",&reference);
        bool valid=(!has_reference && i==0) || resolve_mapping(binding,reference)!=NULL;
        if(i==0) valid=valid && token(doc,"source_object_id",json_object_get_string(field(object,"object_id")));
        json_object_put(doc);if(!valid) return false;
    }
    return true;
}
static bool regions_supported(json_object* row,json_object* object,const CoreAuthoredSurfaceMapping* map) {
    json_object* binding=field(row,"surface_material_binding");
    if(!binding) return layer_maps(row,NULL,token(object,"object_type","mesh_asset_instance"),NULL);
    json_object* maps=field(binding,"mappings");
    if(maps && (!json_object_is_type(maps,json_type_array) || json_object_array_length(maps)>16)) return false;
    for(size_t i=0;maps && i<json_object_array_length(maps);++i) {
        json_object* entry=json_object_array_get_idx(maps,i),*id=field(entry,"id");CoreAuthoredSurfaceMapping parsed;
        if(!json_object_is_type(id,json_type_string) || !json_object_get_string(id)[0] || strlen(json_object_get_string(id))>=64 ||
           !parse(field(entry,"definition"),&parsed) || (parsed.version==3 && !token(object,"object_type","mesh_asset_instance"))) return false;
        for(size_t j=0;j<i;++j) if(token(json_object_array_get_idx(maps,j),"id",json_object_get_string(id))) return false;
        if(field(entry,"path") || field(entry,"sha256")) {
            const char* path=json_object_get_string(field(entry,"path"));
            const char* expected=json_object_get_string(field(entry,"sha256"));char digest[65];
            if(!path || path[0]!='/' || !ray_tracing_sha256_is_valid_hex(expected) ||
               !ray_tracing_sha256_file(path,digest) || strcmp(expected,digest)) return false;
            json_object* definition=json_object_from_file(path);
            bool same=definition && json_object_equal(definition,field(entry,"definition"));
            if(definition) json_object_put(definition);if(!same) return false;
        }
    }
    if(!source_documents_supported(binding,json_object_get_string(field(object,"object_id"))) ||
       !external_mapping_supported(row,object,binding) ||
       !layer_maps(row,binding,token(object,"object_type","mesh_asset_instance"),NULL)) return false;
    json_object* regions=field(binding,"regions");
    if(!regions) return true;
    if(!json_object_is_type(regions,json_type_array) || json_object_array_length(regions)>6 ||
       token(object,"object_type","mesh_asset_instance")) return false;
    bool used[6]={false};
    for(size_t i=0;i<json_object_array_length(regions);++i) {
        json_object* region=json_object_array_get_idx(regions,i);int face=region_face(region);
        json_object* id=field(region,"id");
        if(face<0 || used[face] || (token(object,"object_type","plane_primitive") && face!=0) ||
           !json_object_is_type(id,json_type_string) || !json_object_get_string(id)[0] || strlen(json_object_get_string(id))>=64) return false;
        used[face]=true;
        for(size_t j=0;j<i;++j) if(token(json_object_array_get_idx(regions,j),"id",json_object_get_string(id))) return false;
        CoreAuthoredSurfaceMapping effective=*map;
        json_object* region_map=field(region,"mapping");
        if(field(region,"mapping_ref")) {
            if(region_map) return false;
            region_map=resolve_mapping(binding,field(region,"mapping_ref"));if(!region_map) return false;
        }
        if(region_map && (!parse(region_map,&effective) || effective.version==3)) return false;
        if(field(region,"source") && !source_supported(field(region,"source"),effective.version==2)) return false;
        if(!layer_maps(field(region,"source")?field(region,"source"):row,binding,false,NULL)) return false;
        /* An inherited source must also fit an overridden projection. */
        if(!field(region,"source") && !source_supported(row,effective.version==2)) return false;
    }
    return true;
}
bool RuntimeSurfaceMaterialCompileSource(json_object* row,RuntimeMaterialTextureStack* out) {
    json_object* graph=field(row,"material_graph");if(!graph) graph=field(row,"materialGraph");
    json_object* owned=NULL;
    if(!graph) {
        json_object* layers=source_layers(row);if(!layers) return false;
        owned=json_object_new_object();graph=owned;
        json_object_object_add(graph,"graph_id",json_object_new_string("prepared-source"));
        json_object_object_add(graph,"schema_version",json_object_new_int(1));
        json_object* nodes=json_object_new_array();json_object_object_add(graph,"nodes",nodes);
        for(size_t i=0;i<json_object_array_length(layers);++i) {
            json_object* layer=json_object_array_get_idx(layers,i),*node=json_object_new_object();
            json_object_object_add(node,"node_id",json_object_get(field(layer,"id")));
            json_object_object_add(node,"node_kind",json_object_new_string("layer"));
            json_object_object_add(node,"layer",json_object_get(layer));json_object_array_add(nodes,node);
        }
        json_object_put(layers);
    }
    RuntimeMaterialGraphDocument document;RuntimeMaterialGraphCompileResult compiled;
    bool ok=RuntimeMaterialGraphDocumentFromJsonObject(graph,&document) && RuntimeMaterialGraphCompileToStack(&document,&compiled);
    if(ok) *out=compiled.stack;
    if(owned) json_object_put(owned);return ok;
}
static void prepare_regions(Binding* b,json_object* row) {
    json_object* binding=field(row,"surface_material_binding");
    b->authored=field(row,"authored_texture")!=NULL;
    layer_maps(row,binding,b->mesh,&b->layers);
    json_object* maps=field(binding,"mappings");
    for(size_t i=0;json_object_is_type(maps,json_type_array) && i<json_object_array_length(maps) && i<16;++i) {
        json_object* entry=json_object_array_get_idx(maps,i);
        if(parse(field(entry,"definition"),&b->mappings[i])) {
            snprintf(b->mapping_ids[i],sizeof(b->mapping_ids[i]),"%s",json_object_get_string(field(entry,"id")));b->mapping_count++;
        }
    }
    json_object* regions=field(binding,"regions");
    for(size_t i=0;json_object_is_type(regions,json_type_array) && i<json_object_array_length(regions);++i) {
        json_object* region=json_object_array_get_idx(regions,i);int face=region_face(region);
        if(face<0) continue;
        RegionBinding* r=&b->regions[face];r->active=true;
        json_object* m=field(region,"mapping");if(!m) m=resolve_mapping(binding,field(region,"mapping_ref"));
        r->has_map=m && parse(m,&r->map);
        layer_maps(field(region,"source")?field(region,"source"):row,binding,b->mesh,&r->layers);
        r->has_stack=field(region,"source") && RuntimeSurfaceMaterialCompileSource(field(region,"source"),&r->stack);
    }
}
bool RuntimeSurfaceMappingValidateScene(json_object* root,char* diagnostic,size_t size) {
    json_object* objects=field(root,"objects");
    json_object* rows=field(field(field(field(root,"extensions"),"ray_tracing"),"authoring"),"object_materials");
    for(size_t i=0;json_object_is_type(rows,json_type_array) && i<json_object_array_length(rows);++i) {
        json_object* row=json_object_array_get_idx(rows,i),*binding=NULL;
        if(!json_object_object_get_ex(row,"surface_material_binding",&binding)) continue;
        bool found=false;const char* id=json_object_get_string(field(row,"object_id"));
        for(size_t j=0;id && json_object_is_type(objects,json_type_array) && j<json_object_array_length(objects);++j) {
            json_object* object=json_object_array_get_idx(objects,j);
            if(token(object,"object_id",id) && mapping(object)) found=true;
        }
        if(!found || !json_object_is_type(binding,json_type_object)) {
            snprintf(diagnostic,size,"surface_mapping: M3 material binding needs a mapped object");return false;
        }
    }
    for(size_t i=0;json_object_is_type(objects,json_type_array) && i<json_object_array_length(objects);++i) {
        json_object* object=json_object_array_get_idx(objects,i); json_object* m=NULL;
        json_object* ray=field(field(object,"extensions"),"ray_tracing");
        if(!ray || !json_object_object_get_ex(ray,"surface_mapping",&m)) continue;
        CoreAuthoredSurfaceMapping parsed={0}; double dims[3]={0};
        bool mesh=token(object,"object_type","mesh_asset_instance");
        bool geometry=mesh || ((token(object,"object_type","plane_primitive") || token(object,"object_type","rect_prism_primitive")) && dimensions(object,dims) && frame_supported(object));
        bool valid=geometry &&
            field(object,"object_id") && json_object_is_type(field(object,"object_id"),json_type_string) &&
            strlen(json_object_get_string(field(object,"object_id")))>0 && strlen(json_object_get_string(field(object,"object_id")))<64 &&
            parse(m,&parsed) && (mesh?(parsed.version==2 || parsed.version==3):parsed.version!=3) &&
            layers_supported(root,json_object_get_string(field(object,"object_id")),token(m,"method","axial_height")) &&
            regions_supported(material_row(root,json_object_get_string(field(object,"object_id"))),object,&parsed);
        const char* object_id=json_object_get_string(field(object,"object_id"));
        for(size_t other=0;object_id && other<json_object_array_length(objects);++other)
            if(other!=i && token(json_object_array_get_idx(objects,other),"object_id",object_id)) valid=false;
        json_object* transform=field(object,"transform");
        if(transform && !json_object_is_type(transform,json_type_object)) valid=false;
        if(mesh && field(transform,"pivot_policy") && !token(transform,"pivot_policy","authored_origin")) valid=false;
        for(int g=0;g<2;++g) for(int a=0;a<3;++a) {
            json_object* group=field(transform,g?"rotation":"position");
            if(group && !json_object_is_type(group,json_type_object)) valid=false;
            char key[]={"xyz"[a],0};double value=0;
            if(field(group,key) && !number(field(group,key),&value)) valid=false;
        }
        double world_scale=0;
        if(!number(field(root,"world_scale"),&world_scale) || world_scale<=0) valid=false;
        json_object* scale=field(field(object,"transform"),"scale");
        if(scale && !json_object_is_type(scale,json_type_object)) valid=false;
        for(int a=0;a<3;++a) {
            double value=1; char key[2]={"xyz"[a],0};
            if(field(scale,key) && (!number(field(scale,key),&value) || (parsed.version==3?fabs(value)<1e-9:value<=0))) valid=false;
            if(mesh && (!isfinite(value*world_scale) || fabs(value*world_scale)<1e-9)) valid=false;
            double size=dims[a]*value*world_scale;
            if(!mesh && (a<2 || token(object,"object_type","rect_prism_primitive")) &&
               (!isfinite(size) || size<0.1)) valid=false;
        }
        if(!valid) {
            snprintf(diagnostic,size,"surface_mapping object %zu: invalid surface mapping, geometry, source, region or resource reference",i);
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
        if(token(object,"object_type","mesh_asset_instance")) {
            for(int index=0;index<sceneSettings.objectCount;++index) {
                char id[64]={0};runtime_scene_bridge_get_last_object_id_for_scene_index(index,id,sizeof(id));
                if(!token(object,"object_id",id)) continue;
                Binding* b=&bindings[index];b->active=parse(m,&b->map);b->mesh=true;b->world_scale=world_scale;
                b->asset_graph=field(field(object,"procedural_solid_material_ref"),"graph_path")!=NULL;
                prepare_regions(b,material_row(root,id));
                json_object* transform=field(object,"transform");double angles[3]={0};
                for(int a=0;a<3;++a) {
                    char key[]={"xyz"[a],0};b->scale[a]=1;
                    if(field(field(transform,"scale"),key)) number(field(field(transform,"scale"),key),&b->scale[a]);
                    number(field(field(transform,"position"),key),&b->position[a]);
                    number(field(field(transform,"rotation"),key),&angles[a]);angles[a]*=0.01745329251994329577;
                }
                for(int a=0;a<3;++a) {
                    double x=a==0,y=a==1,z=a==2,t;
                    t=y*cos(angles[0])-z*sin(angles[0]);z=y*sin(angles[0])+z*cos(angles[0]);y=t;
                    t=x*cos(angles[1])+z*sin(angles[1]);z=-x*sin(angles[1])+z*cos(angles[1]);x=t;
                    t=x*cos(angles[2])-y*sin(angles[2]);y=x*sin(angles[2])+y*cos(angles[2]);x=t;
                    b->basis[a][0]=x;b->basis[a][1]=y;b->basis[a][2]=z;
                }
            }
        }
        for(int j=0;j<seeds.primitive_count;++j) {
            const RuntimeSceneBridgePrimitiveSeed* p=&seeds.primitives[j];
            if(!token(object,"object_id",p->object_id) || p->scene_object_index<0 || p->scene_object_index>=MAX_OBJECTS) continue;
            Binding* b=&bindings[p->scene_object_index];
            b->active=parse(m,&b->map) && dimensions(object,b->source_size);
            b->primitive=*p; b->world_scale=world_scale;
            prepare_regions(b,material_row(root,p->object_id));
        }
    }
}
bool RuntimeSurfaceMappingActive(int i) { return animSettings.sceneSource==SCENE_SOURCE_RUNTIME_SCENE && i>=0 && i<MAX_OBJECTS && bindings[i].active; }
unsigned long long RuntimeSurfaceMappingRevision(void) {return revision;}
static bool coordinates_for(const HitInfo3D* hit,const CoreAuthoredSurfaceMapping* m,CoreAuthoredSurfaceCoordinates* out) {
    if(out) memset(out,0,sizeof(*out));
    if(!hit || !out || !RuntimeSurfaceMappingActive(hit->sceneObjectIndex)) return false;
    if(m->version==3) {
        bool ok=hit->hasSurfaceUV && core_authored_surface_uv_coordinates(m,hit->uvSetId,hit->surfaceUV,out);
        if(ok) out->has_tangent=hit->hasSurfaceTangent;return ok;
    }
    const Binding* b=&bindings[hit->sceneObjectIndex];
    const RuntimeSceneBridgePrimitiveSeed* p=&b->primitive;
    double point[3]={hit->position.x/b->world_scale,hit->position.y/b->world_scale,hit->position.z/b->world_scale};
    if(m->space==CORE_AUTHORED_SURFACE_OBJECT_REST && b->mesh) {
        double delta[3];for(int i=0;i<3;++i) delta[i]=point[i]-b->position[i];
        for(int i=0;i<3;++i) point[i]=(delta[0]*b->basis[i][0]+delta[1]*b->basis[i][1]+delta[2]*b->basis[i][2])/b->scale[i];
    } else if(m->space==CORE_AUTHORED_SURFACE_OBJECT_REST) {
        Vec3 d=vec3_sub(hit->position,vec3(p->origin_x,p->origin_y,p->origin_z));
        point[0]=(d.x*p->axis_u_x+d.y*p->axis_u_y+d.z*p->axis_u_z)*b->source_size[0]/p->width;
        point[1]=(d.x*p->axis_v_x+d.y*p->axis_v_y+d.z*p->axis_v_z)*b->source_size[1]/p->height;
        point[2]=p->depth>1e-12 ? (d.x*p->normal_x+d.y*p->normal_y+d.z*p->normal_z)*b->source_size[2]/p->depth : 0;
    }
    return core_authored_surface_coordinates(m,point,out);
}
bool RuntimeSurfaceMappingCoordinates(const HitInfo3D* hit,CoreAuthoredSurfaceCoordinates* out) {
    if(!hit || !RuntimeSurfaceMappingActive(hit->sceneObjectIndex)) return false;
    return coordinates_for(hit,&bindings[hit->sceneObjectIndex].map,out);
}
bool RuntimeSurfaceMappingDefinition(int index,CoreAuthoredSurfaceMapping* out) {
    if(!out || !RuntimeSurfaceMappingActive(index)) return false;
    *out=bindings[index].map;return true;
}
void RuntimeSurfaceMappingBlendPole(const RuntimeMaterialSurfaceEval* base,double weight,RuntimeMaterialSurfaceEval* out) {
    if(weight>=1) return;
#define BLEND(f) out->f=base->f+(out->f-base->f)*weight
    BLEND(colorR);BLEND(colorG);BLEND(colorB);BLEND(roughness);BLEND(reflectivity);
    BLEND(specWeight);BLEND(diffuseWeight);BLEND(transparency);
#undef BLEND
}
bool RuntimeSurfaceMappingEvaluateTiles(int index,double u,double v,const RuntimeMaterialSurfaceEval* base,RuntimeMaterialSurfaceEval* out) {
    RuntimeMaterialTextureStack stack;
    if(!RuntimeSurfaceMappingActive(index) ||
       !SceneEditorMaterialStackGetEffectiveObjectStack(&sceneSettings.sceneObjects[index],index,&stack)) return false;
    const CoreAuthoredSurfaceMapping* m=&bindings[index].map;
    int repeats=m->version==2?(int)round(6.2831853071795864769*m->reference_radius_m/m->tile_m[0]):0;
    return RuntimeMaterialTextureStackEvaluateBrickCellsPeriodic(&stack,u,v,m->seed,repeats,base,out);
}

bool RuntimeSurfaceMappingEvaluate(const SceneObject* object,const HitInfo3D* hit,
    const RuntimeMaterialSurfaceEval* base,RuntimeMaterialSurfaceEval* out) {
    CoreAuthoredSurfaceCoordinates coordinates;
    if(!hit || !RuntimeSurfaceMappingActive(hit->sceneObjectIndex)) return false;
    const Binding* binding=&bindings[hit->sceneObjectIndex];
    const CoreAuthoredSurfaceMapping* map=&binding->map;
    const LayerMappings* layers=&binding->layers;
    RuntimeMaterialSurfaceEval substrate=*base;
    if(binding->authored) {
        int face;double u,v;char reference[64];
        if(binding->mesh || !RuntimeSurfaceMaterialPrimitiveIsland(hit->sceneObjectIndex,hit->position,hit->geometricNormal,&face,&u,&v)) return false;
        if(RuntimeMaterialAuthoredTextureGetMappingReference(hit->sceneObjectIndex,reference,sizeof(reference))) {
            bool found=false;
            for(int i=0;i<binding->mapping_count;++i) if(!strcmp(reference,binding->mapping_ids[i])) {
                CoreAuthoredSurfaceCoordinates q;if(!coordinates_for(hit,&binding->mappings[i],&q)) return false;
                u=q.uv_tiles[0]-floor(q.uv_tiles[0]);v=q.uv_tiles[1]-floor(q.uv_tiles[1]);found=true;break;
            }
            if(!found) return false;
        }
        RuntimeMaterialAuthoredTextureFaceMetadata metadata={0};
        RuntimeMaterialAuthoredTextureGetFaceMetadata(hit->sceneObjectIndex,face,&metadata);
        RuntimeMaterialAuthoredTextureSample image;
        if(!RuntimeMaterialAuthoredTextureSampleFace(hit->sceneObjectIndex,face,u,v,&image)) return false;
        substrate.colorR+=(image.colorR-substrate.colorR)*image.alpha;
        substrate.colorG+=(image.colorG-substrate.colorG)*image.alpha;
        substrate.colorB+=(image.colorB-substrate.colorB)*image.alpha;
        RuntimeMaterialSurfaceApplyAuthoredIntent(&substrate,RuntimeMaterialTextureLayerKindFromStableId(metadata.baseMaterialIntentKind),image.alpha,false);
        if(RuntimeMaterialAuthoredTextureSampleOverlayFace(hit->sceneObjectIndex,face,u,v,&image)) {
            substrate.colorR+=(image.colorR-substrate.colorR)*image.alpha;
            substrate.colorG+=(image.colorG-substrate.colorG)*image.alpha;
            substrate.colorB+=(image.colorB-substrate.colorB)*image.alpha;
            if(!metadata.overlayMaterialIntentKind[0]) RuntimeMaterialAuthoredTextureGetOverlayMaterialIntent(hit->sceneObjectIndex,metadata.overlayMaterialIntentKind,sizeof(metadata.overlayMaterialIntentKind));
            RuntimeMaterialSurfaceApplyAuthoredIntent(&substrate,RuntimeMaterialTextureLayerKindFromStableId(metadata.overlayMaterialIntentKind),image.alpha,true);
        }
        base=&substrate;
    }
    RuntimeMaterialTextureStack stack;
    if(!SceneEditorMaterialStackGetEffectiveObjectStack(object,hit->sceneObjectIndex,&stack)) return false;
    if(!binding->mesh) {
        int face;double u,v;
        if(!RuntimeSurfaceMaterialPrimitiveIsland(hit->sceneObjectIndex,hit->position,hit->geometricNormal,&face,&u,&v)) return false;
        const RegionBinding* region=&binding->regions[face];
        if(region->active) {
            if(region->has_map) map=&region->map;
            if(region->has_stack) stack=region->stack;
            layers=&region->layers;
        }
    }
    bool mapped_layers=false;for(int i=0;i<stack.layerCount;++i) mapped_layers|=layers->active[i];
    if(mapped_layers) {
        RuntimeMaterialMappedLayerSample samples[8];
        for(int i=0;i<stack.layerCount;++i) {
            const CoreAuthoredSurfaceMapping* lm=layers->active[i]?&layers->maps[i]:map;
            if(!coordinates_for(hit,lm,&coordinates)) return false;
            int repeats=lm->version==2?(int)round(6.2831853071795864769*lm->reference_radius_m/lm->tile_m[0]):0;
            samples[i]=(RuntimeMaterialMappedLayerSample){coordinates.uv_tiles[0],coordinates.uv_tiles[1],coordinates.source_weight,lm->seed,repeats};
        }
        return RuntimeMaterialTextureStackEvaluateMappedSamples(&stack,samples,base,out);
    }
    if(!coordinates_for(hit,map,&coordinates)) return false;
    int repeats=map->version==2?(int)round(6.2831853071795864769*map->reference_radius_m/map->tile_m[0]):0;
    if(!RuntimeMaterialTextureStackEvaluateBrickCellsPeriodic(&stack,coordinates.uv_tiles[0],coordinates.uv_tiles[1],map->seed,repeats,base,out)) return false;
    RuntimeSurfaceMappingBlendPole(base,coordinates.source_weight,out);
    return true;
}

bool RuntimeSurfaceMappingEvaluateReferencedStack(const HitInfo3D* hit,const char* reference,
    const RuntimeMaterialTextureStack* stack,const RuntimeMaterialSurfaceEval* base,RuntimeMaterialSurfaceEval* out) {
    if(!hit || !reference || !reference[0] || !RuntimeSurfaceMappingActive(hit->sceneObjectIndex)) return false;
    const Binding* b=&bindings[hit->sceneObjectIndex];
    for(int i=0;i<b->mapping_count;++i) if(!strcmp(reference,b->mapping_ids[i])) {
        const CoreAuthoredSurfaceMapping* m=&b->mappings[i];CoreAuthoredSurfaceCoordinates q;
        if(!coordinates_for(hit,m,&q)) return false;
        int repeats=m->version==2?(int)round(6.2831853071795864769*m->reference_radius_m/m->tile_m[0]):0;
        if(!RuntimeMaterialTextureStackEvaluateBrickCellsPeriodic(stack,q.uv_tiles[0],q.uv_tiles[1],m->seed,repeats,base,out)) return false;
        RuntimeSurfaceMappingBlendPole(base,q.source_weight,out);return true;
    }
    return false;
}

bool RuntimeSurfaceMappingPreviewSupported(int index) {
    if(!RuntimeSurfaceMappingActive(index)) return false;
    if(!bindings[index].asset_graph) return true;
    const RayTracingRuntimeMeshAssetSet *assets=ray_tracing_runtime_mesh_assets_last();
    for(int i=0;assets && i<assets->instance_count;++i) if(assets->instances[i].scene_object_index==index) {
        int a=assets->instances[i].asset_index;
        return a>=0 && a<assets->asset_count && assets->assets[a].procedural_solid_material_runtime_program.valid &&
            assets->assets[a].procedural_solid_material_runtime_program.graph.surface_mapping_ref[0];
    }
    return false;
}

bool RuntimeSurfaceMappingNeedsMeshAttributes(int index) {
    return RuntimeSurfaceMappingActive(index) && (bindings[index].asset_graph || bindings[index].map.version==3);
}
bool RuntimeSurfaceMaterialSampleMesh(int index,int asset_index,size_t triangle,
    const double weights[3],Vec3 world,Vec3 normal,const CoreMeshPreviewLodMesh* lod,
    RuntimeMaterialSurfaceEval* out) {
    if(!RuntimeSurfaceMappingActive(index) || !weights || !lod || triangle>=lod->triangle_count || !out) return false;
    HitInfo3D hit;HitInfo3D_Reset(&hit);hit.sceneObjectIndex=index;hit.localTriangleIndex=(int)triangle;
    hit.triangleIndex=(int)triangle;hit.position=world;hit.normal=hit.geometricNormal=hit.shadingNormal=normal;
    hit.baryU=weights[0];hit.baryV=weights[1];hit.baryW=weights[2];
    if(lod->surface_corners && lod->surface_corner_count==lod->triangle_count*3) {
        hit.hasSurfaceUV=true;memcpy(hit.uvSetId,lod->uv_set_id,sizeof(hit.uvSetId));
        for(int k=0;k<3;++k) for(int a=0;a<2;++a) hit.surfaceUV[a]+=weights[k]*lod->surface_corners[triangle*3+k].uv[a];
    }
    const RayTracingRuntimeMeshAssetSet *assets=ray_tracing_runtime_mesh_assets_last();
    if(bindings[index].asset_graph) {
        if(!assets || asset_index<0 || asset_index>=assets->asset_count || !lod->attribute_protected) return false;
        const ProceduralSolidMaterialRuntimeProgramV1 *program=&assets->assets[asset_index].procedural_solid_material_runtime_program;
        if(!program->valid || !program->graph.surface_mapping_ref[0]) return false;
        hit.hasRegionAuthoredMaterial=true;hit.proceduralSolidMaterialRuntimeProgram=program;
    }
    RuntimeMaterialPayload3D payload;
    if(!RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload)) return false;
    *out=RuntimeMaterialSurfaceEvalMakeBase(payload.baseColorR,payload.baseColorG,payload.baseColorB,
        payload.bsdf.roughness,payload.bsdf.reflectivity,payload.bsdf.specWeight,payload.bsdf.diffuseWeight,payload.transparency);
    out->active=true;return true;
}
