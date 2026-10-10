#include "render/runtime_atmosphere_optics_3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    const double pi = 3.14159265358979323846;
    assert(RuntimeAtmosphereOptics3D_Valid((RuntimeAtmosphereOptics3D){true,.95,.55}));
    assert(!RuntimeAtmosphereOptics3D_Valid((RuntimeAtmosphereOptics3D){true,1.01,0}));
    assert(!RuntimeAtmosphereOptics3D_Valid((RuntimeAtmosphereOptics3D){true,1,NAN}));
    assert(RuntimeAtmosphereOptics3D_SegmentWeight(1,0,2,1)==0);
    assert(RuntimeAtmosphereOptics3D_SegmentWeight(1,1,2,0)==0);
    assert(RuntimeAtmosphereOptics3D_SegmentWeight(1,1,0,1)==0);
    assert(RuntimeAtmosphereOptics3D_SegmentWeight(1,NAN,2,1)==0);
    const double optical_depth = .7, albedo = .93;
    const double exact = albedo * (1-exp(-optical_depth));
    for (int subdivisions=1; subdivisions<=1024; subdivisions*=2) {
        const double ds = 1.0/subdivisions;
        double accumulated=0, transmittance=1;
        for(int i=0;i<subdivisions;++i) {
            accumulated += RuntimeAtmosphereOptics3D_SegmentWeight(
                transmittance,optical_depth,ds,albedo);
            transmittance *= exp(-optical_depth*ds);
        }
        assert(fabs(accumulated-exact)<1e-13);
        assert(accumulated <= 1-transmittance+1e-13);
    }
    assert(RuntimeAtmosphereOptics3D_SegmentWeight(1,1e-14,1,1)>0);
    assert(fabs(RuntimeAtmosphereOptics3D_Phase(.4,0)-1/(4*pi))<1e-15);
    assert(RuntimeAtmosphereOptics3D_Phase(-1,.55) > RuntimeAtmosphereOptics3D_Phase(1,.55));
    for(int k=-3;k<=3;++k) {
        const double g=k*.3;
        double integral=0;
        for(int i=0;i<100000;++i)
            integral += RuntimeAtmosphereOptics3D_Phase(-1+(i+.5)*2/100000,g)*4*pi/100000;
        assert(fabs(integral-1)<2e-6);
        const double view[3]={0,0,1}; double directions[12][3],weights[12],sum=0;
        RuntimeAtmosphereOptics3D_SkyWeights(view,g,directions,weights);
        for(int i=0;i<12;++i) {
            assert(weights[i]>0);
            assert(fabs(directions[i][0]*directions[i][0]+directions[i][1]*directions[i][1]+directions[i][2]*directions[i][2]-1)<1e-15);
            sum+=weights[i];
        }
        assert(fabs(sum-1)<1e-15);
    }
    puts("Atmosphere optics PASS: analytic extinction/scatter coupling, bounds, step invariance, phase normalization, forward direction and uniform-sky quadrature");
    return 0;
}
