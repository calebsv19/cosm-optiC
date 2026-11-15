#include "render/runtime_spectral_surface_path_internal.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(spectral_medium_split(0,0,1,.5).ballistic==1);
    SpectralMediumSplit tiny=spectral_medium_split(0,0x1p-1074,.1,.5);
    assert(tiny.ballistic==1 && tiny.scattered==0 && isfinite(tiny.distance));
    SpectralMediumSplit absorption=spectral_medium_split(.7,0,2,.5);
    assert(absorption.scattered==0 && absorption.ballistic==exp(-1.4));
    for(int c=0;c<4;++c) {
        double sa=.03,ss=c==0?1e-10:c==1?.1:c==2?1:100,st=sa+ss,L=.7;
        double sum=0;
        for(int i=0;i<100000;++i) {
            SpectralMediumSplit s=spectral_medium_split(sa,ss,L,(i+.5)/100000.);
            assert(s.distance>0 && s.distance<L && s.conditional_pdf>0);
            assert(fabs(s.ballistic+s.scattered+sa/st*(-expm1(-st*L))-1)<1e-14);
            assert(fabs(s.scattered*s.conditional_pdf-ss*exp(-st*s.distance))<1e-12);
            sum+=s.scattered*exp(-.4*s.distance)/100000.;
        }
        double reference=ss/(st+.4)*(-expm1(-(st+.4)*L));
        assert(fabs(sum/reference-1)<2e-7);
    }
    RuntimeSpectralSurfacePathConfig config={0};
    assert(spectral_path_max_branches(&config)==1);
    config.glass_split_first=config.medium_split_first=true;
    assert(spectral_path_max_branches(&config)==3);
    config.glass_split_camera_exit=true;
    assert(spectral_path_max_branches(&config)==4);
    config.glass_split_camera_exit=false;config.glass_split_bounded=true;
    assert(spectral_path_max_branches(&config)==6);
    /* Enumerate survival outcomes and two successive continuation choices.
     * The weighted mean must be unchanged even for tiny split branches. */
    for(int fog=0;fog<2;++fog)for(int order=0;order<7;++order) {
        config.fog_survival=fog;
        for(int k=0;k<4;++k) {
            double beta=k==0?.001:k==1?.289:k==2?1:3;
            double survival=spectral_path_roulette_survival(&config,beta,order);
            double expected=fog && order>0?.95:fmin(.95,beta);
            assert(survival==expected && survival>0 && survival<=.95);
            double continued=beta/survival;
            double second=spectral_path_roulette_survival(&config,continued,order);
            assert(fabs(survival*continued-beta)<1e-14);
            assert(fabs(survival*second*(continued/second)-beta)<1e-14);
        }
    }
    PathSignal classified={0};
    spectral_nonscattered_add(&classified,1,1,0,true,0,0);
    spectral_nonscattered_add(&classified,2,1,0,false,1,0);
    spectral_nonscattered_add(&classified,3,2,0,false,0,2);
    spectral_nonscattered_add(&classified,4,3,0,false,1,2);
    spectral_nonscattered_add(&classified,5,3,0,true,1,2);
    spectral_nonscattered_add(&classified,100,0,0,false,0,0);
    spectral_nonscattered_add(&classified,100,3,1,true,1,2);
    for(int k=0;k<5;++k)assert(classified.nonscattered_indirect[k]==k+1);
    PathSignal twice=classified;spectral_path_signal_accumulate(&twice,&classified);
    for(int k=0;k<5;++k)assert(twice.nonscattered_indirect[k]==2*(k+1));
    PathSignal a={.direct=1,.volume=3,.volume_caustic=1,.volume_order={1,1,0,0}},b=a;
    spectral_path_signal_accumulate(&a,&b);
    assert(a.direct==2 && a.volume==6 && a.volume_caustic==2 && a.volume_order[1]==2);
    puts("PASS homogeneous split energy, conditional PDF, independent integral and disjoint signal accumulation");
}
