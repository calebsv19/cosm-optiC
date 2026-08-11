#include "import/compound_scene_ingestion.h"

#include "import/compound_scene_evaluated_scene.h"

#include <stdio.h>
#include <string.h>

#define FNV_OFFSET UINT64_C(1469598103934665603)
#define FNV_PRIME UINT64_C(1099511628211)

static uint64_t bytes(uint64_t h, const void* p, size_t n) { const unsigned char* b=p; for(size_t i=0;i<n;++i){h^=b[i];h*=FNV_PRIME;} return h; }
static void fail(RayCompoundSceneIngestionFailure* f, RayCompoundSceneIngestionFailure v){if(f)*f=v;}
static uint64_t final_geometry_digest(const RayCompoundSceneAssembly* a) {
    uint64_t h=FNV_OFFSET;
    for(size_t i=0;i<a->simulated_count;++i) {
        const RayCompoundSceneDetachedGeometry* g=&a->simulated_geometry[i];
        h=bytes(h,&g->vertex_count,sizeof(g->vertex_count));
        h=bytes(h,g->world_positions,g->vertex_count*sizeof(*g->world_positions));
        h=bytes(h,&g->bounds_min,sizeof(g->bounds_min)); h=bytes(h,&g->bounds_max,sizeof(g->bounds_max));
    }
    return h;
}
void ray_compound_scene_ingestion_descriptor_init(RayCompoundSceneIngestionDescriptor* d,const RayCompoundSceneHandoff* h,const RayCompoundSceneStaticRoom* r){
    if(!d)return; memset(d,0,sizeof(*d)); snprintf(d->schema,sizeof(d->schema),"%s",RAY_COMPOUND_SCENE_INGESTION_SCHEMA);
    if(!h||!r||!ray_compound_scene_handoff_validate(h)||!ray_compound_scene_static_room_validate(r))return;
    d->expected_handoff_digest=h->handoff_digest; d->expected_room_digest=r->artifact_digest; ray_compound_scene_binding_manifest_init(&d->bindings,h);
    for(size_t i=0;i<RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT;++i){
        d->room_visible[i]=i!=RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MAX;
        if(d->room_visible[i]) snprintf(d->room_object_ids[i],sizeof(d->room_object_ids[i]),"sim_room_%zu",i);
        snprintf(d->room_material_ids[i],sizeof(d->room_material_ids[i]),"mat_sim_room_%zu",i);
    }
}
bool ray_compound_scene_ingestion_descriptor_validate(const RayCompoundSceneIngestionDescriptor* d,const RayCompoundSceneHandoff* h,const RayCompoundSceneStaticRoom* r){
    if(!d||!h||!r||strcmp(d->schema,RAY_COMPOUND_SCENE_INGESTION_SCHEMA)||d->expected_handoff_digest!=h->handoff_digest||d->expected_room_digest!=r->artifact_digest||d->tick>=RAY_COMPOUND_SCENE_HANDOFF_MAX_FRAMES||!ray_compound_scene_binding_manifest_validate(&d->bindings,h))return false;
    for(size_t i=0;i<RAY_COMPOUND_SCENE_STATIC_ROOM_SURFACE_COUNT;++i)
        if(!d->room_material_ids[i][0]||d->room_visible[i]!=(i!=RAY_COMPOUND_SCENE_STATIC_ROOM_Z_MAX)||
           (d->room_visible[i]&&!d->room_object_ids[i][0])) return false;
    return true;
}
uint64_t ray_compound_scene_ingestion_result_digest(const RayCompoundSceneIngestionResult* r){
    uint64_t h=FNV_OFFSET; if(!r||!r->valid)return 0; h=bytes(h,&r->handoff_digest,sizeof(r->handoff_digest));h=bytes(h,&r->room_digest,sizeof(r->room_digest));h=bytes(h,&r->basis_digest,sizeof(r->basis_digest));h=bytes(h,&r->tick,sizeof(r->tick));h=bytes(h,&r->room.geometry_digest,sizeof(r->room.geometry_digest));h=bytes(h,&r->assembly.assembly_digest,sizeof(r->assembly.assembly_digest));return bytes(h,&r->snapshot,sizeof(r->snapshot));
}
bool ray_compound_scene_ingestion_resolve_exact(const RayCompoundSceneIngestionDescriptor* d,const RayCompoundSceneHandoff* h,const RayCompoundSceneStaticRoom* r,const RayEvaluatedSceneSnapshot* base,RayCompoundSceneAssemblyRequest* req,RayCompoundSceneIngestionResult* out,RayCompoundSceneIngestionFailure* f){
    RayCompoundSceneRoomBasis basis; RayCompoundSceneMappedRoom mapped; RayCompoundSceneIngestionResult c={0}; RayCompoundSceneAssemblyFailure af; RayCompoundSceneRoomGeometryFailure rf; RayCompoundSceneEvaluatedSceneFailure ef;
    fail(f,RAY_COMPOUND_SCENE_INGESTION_FAILURE_NONE); if(!d||!h||!r||!base||!req||!out){fail(f,RAY_COMPOUND_SCENE_INGESTION_FAILURE_INPUT);return false;}
    if(!ray_compound_scene_ingestion_descriptor_validate(d,h,r)){fail(f,RAY_COMPOUND_SCENE_INGESTION_FAILURE_DESCRIPTOR);return false;}
    ray_compound_scene_room_basis_init(&basis);
    if(!ray_compound_scene_room_basis_bind(h,r,&basis,&mapped,NULL)||!ray_compound_scene_room_geometry_build(h,r,&basis,&mapped,&c.room,&rf)){fail(f,RAY_COMPOUND_SCENE_INGESTION_FAILURE_PROVENANCE);return false;}
    for(size_t i=0;i<c.room.plane_count;++i){c.room.planes[i].render_visible=d->room_visible[i];snprintf(c.room.planes[i].material_id,sizeof(c.room.planes[i].material_id),"%s",d->room_material_ids[i]);}
    c.room.geometry_digest=ray_compound_scene_room_geometry_digest(&c.room);
    if(!ray_compound_scene_evaluated_scene_apply_ingestion_exact(h,&d->bindings,d->tick,base,&c.snapshot,&ef)){fail(f,RAY_COMPOUND_SCENE_INGESTION_FAILURE_RESOLUTION);return false;}
    req->handoff=h; req->manifest=&d->bindings; req->snapshot=&c.snapshot;
    if(!ray_compound_scene_assembly_build_exact(req,&c.assembly,&af)){fail(f,RAY_COMPOUND_SCENE_INGESTION_FAILURE_RESOLUTION);return false;}
    /* Map the final, owned geometry before publishing it; the result digest
       below therefore covers the same coordinates the future runtime sees. */
    for(size_t i=0;i<c.assembly.simulated_count;++i){RayCompoundSceneDetachedGeometry* g=&c.assembly.simulated_geometry[i];for(size_t j=0;j<g->vertex_count;++j)g->world_positions[j]=ray_compound_scene_room_basis_map_vec3(&basis,g->world_positions[j]);g->bounds_min=g->bounds_max=g->world_positions[0];for(size_t j=1;j<g->vertex_count;++j){RayCompoundSceneVec3 v=g->world_positions[j];if(v.x<g->bounds_min.x)g->bounds_min.x=v.x;if(v.y<g->bounds_min.y)g->bounds_min.y=v.y;if(v.z<g->bounds_min.z)g->bounds_min.z=v.z;if(v.x>g->bounds_max.x)g->bounds_max.x=v.x;if(v.y>g->bounds_max.y)g->bounds_max.y=v.y;if(v.z>g->bounds_max.z)g->bounds_max.z=v.z;}}
    c.valid=true;c.handoff_digest=h->handoff_digest;c.room_digest=r->artifact_digest;c.basis_digest=basis.basis_digest;c.tick=d->tick;c.result_digest=ray_compound_scene_ingestion_result_digest(&c)^final_geometry_digest(&c.assembly);*out=c;return true;
}
