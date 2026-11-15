#include "render/runtime_spectral_surface_path_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>

static void coverage(void) {
    for(unsigned seed=0;seed<16;++seed)for(unsigned ax=0;ax<=6;++ax) {
        int bins[64]={0};
        for(unsigned i=0;i<64;++i) {
            PathRandom r=spectral_path_random_init(9123,seed,i,true);
            double u=spectral_path_random(&r),v=spectral_path_random(&r);
            assert(u>0 && u<1 && v>0 && v<1);
            unsigned x=(unsigned)(u*(1u<<ax)),y=(unsigned)(v*(1u<<(6-ax)));
            ++bins[y*(1u<<ax)+x];
        }
        for(int k=0;k<64;++k)assert(bins[k]==1);
    }
}
static void projections(void) {
    /* The rejected repeated-Sobol-pair construction fails this cross-event
     * octant test despite passing every individual 2D dyadic coverage test. */
    for(unsigned seed=0;seed<16;++seed) {
        int bins[8]={0};
        for(unsigned i=0;i<4096;++i) {
            PathRandom r=spectral_path_random_init(9123,seed,i,true);unsigned cell=0;
            for(unsigned e=0;e<3;++e) {
                spectral_path_random_event(&r,e);
                double u=spectral_path_random(&r);assert(u>0 && u<1);
                cell|=(u>=.5)<<e;
            }
            ++bins[cell];
        }
        for(int k=0;k<8;++k)assert(bins[k]>=384 && bins[k]<=640);
        int bins5[5]={0};
        for(unsigned i=0;i<5;++i) {
            PathRandom r=spectral_path_random_init(9123,seed,i,true);
            spectral_path_random_event(&r,0);++bins5[(unsigned)(spectral_path_random(&r)*5)];
        }
        for(int k=0;k<5;++k)assert(bins5[k]==1);
    }
}
static void identity(void) {
    for(unsigned seed=0;seed<16;++seed)for(unsigned i=0;i<127;++i) {
        PathRandom a=spectral_path_random_init(9876,seed,i,false);
        uint64_t key=((uint64_t)seed<<32)^(UINT64_C(9876)*UINT64_C(0x9e3779b97f4a7c15))^((uint64_t)i*UINT64_C(0x94d049bb133111eb));
        for(unsigned d=1;d<=32;++d) {
            spectral_path_random_event(&a,d);
            assert(spectral_path_random(&a)==RuntimeSpectral_Uniform64(key^((uint64_t)d*UINT64_C(0xd1b54a32d192ed03))));
        }
        PathRandom b=spectral_path_random_init(9876,seed,i,true),c=b;
        double x=spectral_path_random(&b);spectral_path_random_fork(&c,UINT64_C(0xe7037ed1a0b428db));
        assert(x!=spectral_path_random(&c));
        b=spectral_path_random_init(9876,seed,i,true);assert(x==spectral_path_random(&b));
        spectral_path_random_event(&b,3);c=spectral_path_random_init(9876,seed,i,true);
        for(int d=0;d<11;++d)(void)spectral_path_random(&c);
        spectral_path_random_event(&c,3);assert(spectral_path_random(&b)==spectral_path_random(&c));
    }
}
static void audit(void) {
    for(int limit=1;limit<=16;++limit) {
        RuntimeSpectralSurfacePathReadout out={0};
        for(int i=1;i<=20;++i) {
            PathSignal p={.direct=i,.volume=2*i,.volume_caustic=.5*i,.caustic=3*i,.volume_order={1.5*i,0,0,0}};
            spectral_path_audit_sample(&out,limit,i,550,1,.1,.2,vec3(1,2,3),&p,1,2);
        }
        assert(out.sample_audit_count==limit && out.sample_audit_observed==20);
        assert(out.sample_audit_sum.y==2520);
        for(int i=0;i<limit;++i) {assert(out.sample_audit[i].sample==20-i);assert(out.sample_audit[i].component_y[0]==12*(20-i));}
        RuntimeSpectralSurfacePathConfig p={.sample_audit_enabled=true,.sample_audit_limit=limit};
        json_object *j=spectral_path_audit_json(&p,&out);assert(j);
        assert(json_object_array_length(json_object_object_get(j,"top_samples"))==(size_t)limit);json_object_put(j);
    }
}
int main(void) {coverage();projections();identity();audit();puts("PASS primary Sobol coverage, distinct transport projections, legacy identity, branch/event separation and bounded audit accounting");}
