/* Test-only observer of Ball's unmodified eight-body runtime. The public contact
 * activity scalars omit signed vectors and warm starts; do not sum them as net
 * external impulses. Reconstruct current solved impulses from cache points. */
#include "interop/rigid_scene_runtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static BallCollisionVec3 add(BallCollisionVec3 a,BallCollisionVec3 b) {
    return ball_collision_vec3(a.x+b.x,a.y+b.y,a.z+b.z);
}
static BallCollisionVec3 scale(BallCollisionVec3 a,double s) {
    return ball_collision_vec3(a.x*s,a.y*s,a.z*s);
}
static BallCollisionVec3 momentum(const BallRigidSceneRuntime *s,double *mass) {
    BallCollisionVec3 p=ball_collision_vec3(0,0,0);*mass=0;
    for(size_t i=0;i<s->body_count;++i) {
        const BallRigidBody3D *b=&s->bodies[i].body;
        if(!b->is_static){*mass+=b->mass_kg;p=add(p,scale(b->velocity_mps,b->mass_kg));}
    }
    return p;
}
static int dynamic(const BallRigidSceneRuntime *s,int id) {
    for(size_t i=0;i<s->body_count;++i)
        if(s->bodies[i].body.id==id)return !s->bodies[i].body.is_static;
    fprintf(stderr,"unrecognized cache body %d\n",id);exit(3);
}
static void vec(BallCollisionVec3 v) { printf("[%.17g,%.17g,%.17g]",v.x,v.y,v.z); }
int main(int argc,char **argv) {
    if(argc!=3 && argc!=4)return 2;
    const int diagnostic_hz=argc==4?atoi(argv[3]):0;
    if(argc==4 && diagnostic_hz!=120 && diagnostic_hz!=240 && diagnostic_hz!=480)return 2;
    BallRigidSceneRuntime *s=ball_rigid_scene_runtime_open(argv[1],argv[2]);
    if(!s){fprintf(stderr,"runtime admission failed\n");return 2;}
    /* Test-only time refinement below the native delivery boundary. Never
     * export these observations as immutable native240 results. Input seconds
     * are preserved; production admission and scene rate are unchanged. */
    if(diagnostic_hz){
        const double seconds=(double)s->contract.end_tick *
            s->contract.rate_denominator/s->contract.rate_numerator;
        s->loop.policy.fixed_dt_seconds=1./diagnostic_hz;
        s->contract.end_tick=(unsigned long)(seconds*diagnostic_hz);
    }
    double mass;BallCollisionVec3 p=momentum(s,&mass);unsigned long tick=0;
    while(tick<s->contract.end_tick) {
        const BallCollisionVec3 before=p;
        if(!ball_rigid_scene_runtime_step(s)){fprintf(stderr,"%s\n",s->error);return 2;}
        ++tick;p=momentum(s,&mass);
        BallCollisionVec3 external=ball_collision_vec3(0,0,0);
        for(size_t k=0;k<BALL_RIGID3D_COMPOUND_MAX_MANIFOLDS_PER_PAIR;++k) {
            const BallRigid3DContactCache *c=&s->world->manifold_caches[k];
            for(size_t j=0;j<BALL_RIGID3D_CONTACT_CACHE_MAX_PAIRS;++j) {
                const BallRigid3DContactCachePair *pair=&c->pairs[j];
                if(!pair->active || pair->last_seen_frame!=c->current_frame)continue;
                const int sign=dynamic(s,pair->body_id_high)-dynamic(s,pair->body_id_low);
                if(!sign)continue; /* Both dynamic: equal/opposite internal impulse. */
                for(size_t n=0;n<BALL_RIGID3D_CONTACT_CACHE_MAX_POINTS;++n) {
                    const BallRigid3DContactCachePoint *point=&pair->points[n];
                    if(!point->active || point->last_seen_frame!=c->current_frame)continue;
                    external=add(external,scale(add(scale(pair->canonical_normal_low_to_high,
                        point->accumulated_normal_impulse),point->accumulated_tangent_impulse_low_to_high),sign));
                }
            }
        }
        printf("{\"tick\":%lu,\"before_kg_mps\":",tick);vec(before);
        printf(",\"after_kg_mps\":");vec(p);
        printf(",\"static_reaction_kg_mps\":");vec(external);
        printf(",\"dynamic_mass_kg\":%.17g",mass);
        if(diagnostic_hz){
            cJSON *snapshot=ball_rigid_scene_runtime_snapshot(s);
            char *bytes=cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(snapshot,"bodies"));
            if(!bytes){cJSON_Delete(snapshot);ball_rigid_scene_runtime_free(s);return 2;}
            printf(",\"diagnostic_only_hz\":%d,\"bodies\":%s",diagnostic_hz,bytes);
            free(bytes);cJSON_Delete(snapshot);
        }
        printf("}\n");
    }
    ball_rigid_scene_runtime_free(s);return 0;
}
