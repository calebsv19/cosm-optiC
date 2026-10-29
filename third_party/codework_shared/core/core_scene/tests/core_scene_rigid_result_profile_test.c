#include "core_scene_rigid_result.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Deterministic digest stand-in isolates result-match structural admission.
 * Real digest identity and coordinate matching are covered by sealed consumer tests. */
static bool digest(const void *p,size_t n,char hex[65]){assert(p&&n);memset(hex,'a',64);hex[64]=0;return true;}
int main(void){
 CoreSceneRigidContract c={0};strcpy(c.scene_id,"compound");c.schema_version=1;c.rate_numerator=240;c.rate_denominator=1;c.end_tick=1;c.object_count=c.asset_count=c.binding_count=1;
 CoreSceneRigidObject *o=&c.objects[0];strcpy(o->id,"body");strcpy(o->mesh_id,"mesh");o->mesh_instance=true;o->scale[0]=o->scale[1]=o->scale[2]=1;
 CoreSceneRigidAsset *a=&c.assets[0];strcpy(a->id,"collision");strcpy(a->source_id,"source");strcpy(a->mesh_id,"mesh");memset(a->source_sha256,'a',64);memset(a->collision_sha256,'b',64);memset(a->mesh_sha256,'c',64);a->shape=CORE_SCENE_RIGID_CONVEX_COMPOUND;a->principal_to_source[0]=a->principal_to_source[4]=a->principal_to_source[8]=1;a->inertia_per_kg[0]=a->inertia_per_kg[1]=a->inertia_per_kg[2]=1;
 CoreSceneRigidBinding *b=&c.bindings[0];strcpy(b->object_id,"body");strcpy(b->asset_id,a->id);strcpy(b->source_id,a->source_id);strcpy(b->mesh_id,a->mesh_id);strcpy(b->source_sha256,a->source_sha256);strcpy(b->collision_sha256,a->collision_sha256);strcpy(b->mesh_sha256,a->mesh_sha256);memcpy(b->principal_to_source,a->principal_to_source,sizeof(b->principal_to_source));b->body_id=17;b->role=CORE_SCENE_RIGID_DYNAMIC;b->owner=CORE_SCENE_RIGID_SIMULATION;b->collides=true;b->mass_kg=1;b->principal_inertia_kgm2[0]=b->principal_inertia_kgm2[1]=b->principal_inertia_kgm2[2]=1;b->orientation_wxyz[0]=1;
 CoreSceneRigidPose pose={0};CoreSceneRigidResult r={0};r.poses=&pose;r.body_count=1;r.bindings[0].physical.body_id=17;memset(r.physical_input_sha256,'a',64);
 for(unsigned children=1;children<=13;++children){a->child_count=children;CoreSceneRigidContract before=c;bool ok=core_scene_rigid_result_match(&r,&c,digest).code==CORE_OK;assert(ok==(children<=12));assert(!memcmp(&before,&c,sizeof(c)));}
 puts("rigid result profile: one through twelve children accepted; thirteen refused; input unchanged");return 0;
}
