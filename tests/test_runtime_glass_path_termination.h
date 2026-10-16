#pragma once
#include "render/runtime_glass_path_termination_3d.h"
static void test_runtime_glass_path_termination(void) {
    Vec3 color = vec3(.2, .5, 1);
    assert_close("glass_rr_prefix_exact", RuntimeGlassPathTermination3D_Survival(8,8,color,1),1,0);
    assert_close("glass_rr_disabled_lobe", RuntimeGlassPathTermination3D_Survival(0,1,color,1),0,0);
    assert_close("glass_rr_eta_cancels", RuntimeGlassPathTermination3D_Survival(8,9,vec3(1/2.25,1/2.25,1/2.25),2.25),.98,1e-14);
    assert_close("glass_rr_absorbing", RuntimeGlassPathTermination3D_Survival(8,9,vec3(.1,.2,.3),1),.3,1e-14);
    assert_close("glass_rr_trapped_survival_bounded", RuntimeGlassPathTermination3D_Survival(64,999,color,1),.98,0);
    Vec3 unchanged=color;
    assert_true("glass_rr_boundary_rejects", !RuntimeGlassPathTermination3D_Apply(.5,.5,&unchanged));
    assert_close("glass_rr_no_mutation_on_termination",unchanged.z,1,0);
    double mean[3]={0};
    for(int i=0;i<10000;++i) {
        Vec3 beta=color;
        if(RuntimeGlassPathTermination3D_Apply(.4,(i+.5)/10000.,&beta)) {
            mean[0]+=beta.x;mean[1]+=beta.y;mean[2]+=beta.z;
        }
    }
    assert_close("glass_rr_rgb_expectation_r",mean[0]/10000,color.x,1e-12);
    assert_close("glass_rr_rgb_expectation_g",mean[1]/10000,color.y,1e-12);
    assert_close("glass_rr_rgb_expectation_b",mean[2]/10000,color.z,1e-12);
    /* Analytic geometric tail: every step escapes with probability 0.1.
     * A black cutoff at 8 loses .9^8 = 43% of its unit expectation.
     * Both roulette prefixes must instead reproduce unity. The escape RNG is
     * independent of the production roulette stream; paths may exceed 128. */
    for(int prefix=2;prefix<=64;prefix*=4) {
        double sum=0; unsigned rng=0x7e915d;
        for(int i=0;i<200000;++i) {
            RuntimeNative3DSamplingContext sampling={(unsigned)i, (unsigned short)(i%32),32};
            Vec3 beta=vec3(1,1,1);
            for(unsigned step=1;step<10000;++step) {
                rng=1664525u*rng+1013904223u;
                if((rng+.5)/4294967296. < .1) {sum+=beta.x;break;}
                double survival=RuntimeGlassPathTermination3D_Survival(prefix,step,beta,1);
                if(!RuntimeGlassPathTermination3D_Apply(survival,
                    RuntimeGlassPathTermination3D_Sample(&sampling,901,i,step),&beta))break;
                assert_true("glass_rr_long_tail_finite",isfinite(beta.x));
            }
        }
        assert_close("glass_rr_geometric_tail_unit_expectation",sum/200000,1,.006);
    }
    /* Each index must see the full random range, rather than remaining in its
     * Stratified2D column at every boundary. */
    for(int index=0;index<32;++index) {
        RuntimeNative3DSamplingContext sampling={3201,0,32};
        int low=0,high=0;double cross=0,average=0;
        for(unsigned b=0;b<10000;++b) {
            double u=RuntimeGlassPathTermination3D_Sample(&sampling,991,index,b);
            double v=RuntimeGlassPathTermination3D_Sample(&sampling,991,index,b+1);
            low+=u<.25;high+=u>.75;average+=u;cross+=(u-.5)*(v-.5);
        }
        assert_true("glass_rr_each_index_full_range",low>2200&&high>2200);
        assert_close("glass_rr_stream_mean",average/10000,.5,.012);
        assert_close("glass_rr_stream_adjacent_covariance",cross/10000,0,.004);
    }
}
