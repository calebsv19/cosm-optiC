#include "render/runtime_spectral_surface_path_internal.h"
#include <assert.h>
#include <stdio.h>
static uint64_t state=12345;
static double uniform(void) {
    state^=state<<13;state^=state>>7;state^=state<<17;
    return (state>>11)*0x1p-53;
}
static double power(double a,double b) { return a*a/(a*a+b*b); }
static bool hits(Vec3 at,Vec3 w) {
    /* Independent plane/rectangle intersection for the test oracle. */
    if(w.z>=0)return false;
    double t=-at.z/w.z;
    return fabs(at.x+t*w.x)<=1 && fabs(at.y+t*w.y)<=.9;
}
static double corner_integral(double x,double y,double h) {
    return atan2(x*y,h*sqrt(x*x+y*y+h*h));
}
static double rectangle_integral(double x0,double x1,double y0,double y1,double h) {
    return corner_integral(x1,y1,h)-corner_integral(x0,y1,h)-corner_integral(x1,y0,h)+corner_integral(x0,y0,h);
}
static void rectangle_cdf(RuntimeSpectralSurfacePath *context) {
    RuntimeSpectralSurfacePathConfig *p=(RuntimeSpectralSurfacePathConfig *)&context->profile->surface_path;
    p->volume_rectangle_mis=true;p->volume_light_only=false;p->volume_light_mis=false;
    Vec3 points[]={ {0,0,.01},{2,.7,.001},{.5,-.8,.1},{0,0,100},{-2,1,.01},{0,0,1e-7},{0,0,1e6},{1,.9,.0001} };
    for(unsigned k=0;k<sizeof(points)/sizeof(points[0]);++k) {
        Vec3 at=points[k];SpectralVolumeProposal q=spectral_volume_proposal(context,at,2);
        if(k==5){assert(!q.active);continue;}
        assert(q.active);
        if(k==6){assert(!q.rectangle);assert(q.pdf>0);continue;}
        assert(q.rectangle);
        double x0=-1-at.x,x1=1-at.x,y0=-.9-at.y,y1=.9-at.y,h=at.z;
        double area=rectangle_integral(x0,x1,y0,y1,h);
        assert(fabs(q.pdf*area-1)<1e-10);
        /* Independent marginal integral and conditional CDF; checks off-axis,
         * near-plane, edge, distant and grazing configurations. */
        for(int i=0;i<64;++i)for(int j=0;j<64;++j) {
            double u=(i+.5)/64,v=(j+.5)/64;
            Vec3 w=spectral_volume_sample_proposal(&q,u,v);
            double t=-h/w.z,x=t*w.x,y=t*w.y,D=hypot(x,h);
            assert(hits(at,w));assert(spectral_volume_light_pdf(&q,w)==q.pdf);
            double cdf=rectangle_integral(x0,x,y0,y1,h)/area;
            double lo=y0/hypot(D,y0),hi=y1/hypot(D,y1),cy=(y/hypot(D,y)-lo)/(hi-lo);
            assert(fabs(cdf-u)<1e-6);assert(fabs(cy-v)<1e-6);
        }
        for(int i=0;i<2;++i)for(int j=0;j<2;++j) {
            Vec3 w=spectral_volume_sample_proposal(&q,i,j);
            assert(isfinite(w.x) && isfinite(w.y) && isfinite(w.z));
        }
        assert(spectral_volume_light_pdf(&q,vec3(0,0,1))==0);
        printf("PASS independent rectangle CDF geometry %u\n",k);
    }
    /* Geometry-only near-plane fallback remains a normalized cap. */
    SpectralVolumeProposal q=spectral_volume_proposal(context,vec3(0,0,2e-7),2);
    assert(q.active && !q.rectangle && q.pdf>0);
    p->volume_rectangle_mis=false;
}
int main(void) {
    RuntimeSpectralPencil profile={0};
    RuntimeSpectralSurfacePath context={.profile=&profile};
    RuntimeSpectralSurfacePathConfig *p=&profile.surface_path;
    p->volume_light_mis=true;p->origin=vec3(0,0,0);p->normal=vec3(0,0,1);
    p->axis_u=vec3(1,0,0);p->axis_v=vec3(0,1,0);p->width=2;p->height=1.8;
    assert(!spectral_volume_proposal(&context,vec3(0,0,1),1).active);
    assert(!spectral_volume_proposal(&context,vec3(0,0,-1),2).active);
    p->sky=true;assert(!spectral_volume_proposal(&context,vec3(0,0,1),2).active);p->sky=false;
    p->native_sources=true;assert(!spectral_volume_proposal(&context,vec3(0,0,1),2).active);p->native_sources=false;
    rectangle_cdf(&context);
    const int N=262144;
    for(int light_only=0;light_only<3;++light_only)for(int near=0;near<3;++near) {
        p->volume_light_only=light_only==1;p->volume_light_mis=light_only==0;p->volume_rectangle_mis=light_only==2;
        double h=near==0?.01:near==1?.2:2;
        Vec3 at=vec3(0,0,h),incoming=vec3(0,0,-1);
        SpectralVolumeProposal q=spectral_volume_proposal(&context,at,2);assert(q.active);
        /* All rectangle points must lie in the normalized cap support. */
        for(int i=0;i<=32;++i)for(int j=0;j<=32;++j) {
            Vec3 w=vec3_normalize(vec3_sub(vec3(-1+i/16.,-.9+j*.9/16.,0),at));
            assert(spectral_volume_light_pdf(&q,w)>0);
        }
        for(int anisotropy=0;anisotropy<3;++anisotropy) {
            double g=anisotropy==0?0:anisotropy==1?.6:-.6;
            double mass=0,mass2=0,moment=0;
            for(int k=0;k<N;++k) {
                double pdf,f;Vec3 w=spectral_volume_sample_direction(g,incoming,&q,uniform(),uniform(),&pdf,&f);
                assert(pdf>0 && isfinite(pdf));assert(fabs(vec3_length(w)-1)<1e-12);
                assert(pdf==spectral_volume_direction_pdf(g,incoming,w,&q));
                if(light_only==1) {
                    SpectralVolumeProposal legacy={0};double legacy_pdf,legacy_f;
                    double u=uniform(),v=uniform();
                    Vec3 aa=spectral_volume_sample_direction(g,incoming,&q,u,v,&pdf,&f);
                    Vec3 bb=spectral_volume_sample_direction(g,incoming,&legacy,u,v,&legacy_pdf,&legacy_f);
                    assert(aa.x==bb.x && aa.y==bb.y && aa.z==bb.z && pdf==legacy_pdf && f==legacy_f);
                    assert(f/pdf==1);
                }
                double weight=f/pdf;mass+=weight;mass2+=weight*weight;moment+=vec3_dot(w,incoming);
            }
            double se=sqrt(fmax(0,mass2/N-pow(mass/N,2))/N);
            assert(fabs(mass/N-1)<6*se+.0001);
            double expected=light_only==1?g:.75*g+.25*(1-q.one_minus_cosine/2)*vec3_dot(q.axis,incoming);
            if(light_only!=2)assert(fabs(moment/N-expected)<.006);
        }
        /* Isotropic energy to a rectangle has an independent analytic solid angle.
         * Both MIS techniques must sum to it, including null cap samples. */
        double sum=0,sum2=0;
        for(int k=0;k<N;++k) {
            Vec3 l=spectral_volume_sample_proposal(&q,uniform(),uniform());
            double lpdf=q.pdf,cpdf=spectral_volume_direction_pdf(0,incoming,l,&q),f=1/(4*M_PI);
            double value=hits(at,l)?f/lpdf*power(lpdf,cpdf):0;
            double pdf,pf;Vec3 w=spectral_volume_sample_direction(0,incoming,&q,uniform(),uniform(),&pdf,&pf);
            double other=spectral_volume_light_pdf(&q,w);
            if(hits(at,w))value+=pf/pdf*power(pdf,other);
            sum+=value;sum2+=value*value;
        }
        double reference=atan(.9/(h*sqrt(h*h+1+.81)))/M_PI;
        double mean=sum/N,se=sqrt(fmax(0,sum2/N-mean*mean)/N);
        assert(fabs(mean-reference)<6*se+1e-5);
        printf("PASS rectangle h=%g: MIS %.9g reference %.9g SE %.3g\n",h,mean,reference,se);
    }
    puts("PASS cone support, phase importance energy, mixture moments and two-technique MIS integral");
}
