/* Qualification-only observer: actual admitted runtime masses and inertias. */
#include "interop/rigid_scene_runtime.h"
#include <stdio.h>
int main(int argc,char **argv) {
    if(argc!=3)return 2;
    BallRigidSceneRuntime *s=ball_rigid_scene_runtime_open(argv[1],argv[2]);
    if(!s)return 2;
    for(size_t i=0;i<s->body_count;++i){
        const BallRigidBody3D *b=&s->bodies[i].body;
        const CoreSceneRigidBinding *binding=&s->contract.bindings[s->binding_indices[i]];
        printf("{\"object_id\":\"%s\",\"body_id\":%d,\"mass_kg\":%.17g,"
               "\"principal_inertia_kgm2\":[%.17g,%.17g,%.17g]}\n",
               binding->object_id,b->id,b->mass_kg,b->inertia_kg_m2.x,
               b->inertia_kg_m2.y,b->inertia_kg_m2.z);
    }
    ball_rigid_scene_runtime_free(s);return 0;
}
