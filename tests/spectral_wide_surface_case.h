#include "render/runtime_spectral_surface_map_internal.h"
#include "render/runtime_caustic_photon_surface_kernel_3d.h"
static void wide_surface_cases(void) {
    for(int guarded=0;guarded<2;guarded++) {
        RuntimePrimitive3D planes[2];RuntimeScene3D scene=plane_scene(&planes[0],1,2);
        if(guarded) { RuntimeScene3D duplicate=plane_scene(&planes[1],1,2);(void)duplicate;scene.primitiveCount=2; }
        RuntimeSpectralPencil p={.receiver_radius_m=.01};p.map.adaptive_scattered=true;p.map.scattered_support_m=.08;p.map.scattered_neighbors=16;
        RuntimeSpectralSurfaceMap *m=calloc(1,sizeof(*m));assert(m);m->mask=1023;m->count=289;m->records=calloc(m->count,sizeof(*m->records));m->heads=malloc(1024*sizeof(int));assert(m->records&&m->heads);
        for(int i=0;i<1024;i++)m->heads[i]=-1;
        for(int i=0;i<m->count;i++) {
            Deposit *d=&m->records[i];d->position=vec3(1,(i%17-8)*.02+.5,(i/17-8)*.02+.5);if(i==288)d->position=vec3(1,1.495,.5);if(i==287)d->position=vec3(1,1.48,.5);d->primitive=0;d->front=i%7!=0;d->band=i%2?100:0;d->watts=1./m->count;d->scattered=i%5!=0;d->scale=1;
            assert(spectral_surface_cell(&planes[0].shape.plane,d->position,.01,&d->u,&d->v));size_t b=spectral_surface_bucket(m,0,d->band,d->u,d->v);d->next=m->heads[b];m->heads[b]=i;
        }
        int expanded=0;assert(spectral_surface_wide_prepare(m,&p,&scene,&expanded));assert((expanded>0)==!guarded);m->has_expanded=expanded>0;
        double worst=0;int nonzero=0;
        for(int y=0;y<43;y++)for(int z=0;z<47;z++)for(int b=0;b<2;b++) {
            RuntimeSpectralPathReadout hit={.receiver=true,.receiver_front=true,.primitive=0,.position={1,-.29+y*.037,-.29+z*.034}};
            unsigned long long candidates=0;int band=b?100:0;double actual=RuntimeSpectralSurfaceMap_Radiance(m,&p,&scene,&hit,band,&candidates),reference=0;
            for(int j=0;j<m->count;j++)if(m->records[j].band==band && m->records[j].front) {
                const Deposit *d=&m->records[j];reference+=d->watts*RuntimeCausticPhotonSurfaceKernel3D_Weight(vec3_length(vec3_sub(d->position,hit.position)),d->scale*.01)/M_PI;
            }
            double error=fabs(actual-reference);if(error>worst)worst=error;assert(error<1e-12);nonzero+=actual>0;
        }
        assert(nonzero>0);
        for(int j=0;j<m->count;j++) {
            const Deposit *d=&m->records[j];if(!d->scattered || j==288)assert(d->scale==1);
            double h=d->scale*.01,q[]={.21132486540518713,.7886751345948129},mass=0;
            for(int k=0;k<2;k++){double r=h*q[k];mass+=.5*h*2*M_PI*r*RuntimeCausticPhotonSurfaceKernel3D_Weight(r,h);}
            assert(fabs(mass-1)<1e-12);
        }
        printf("wide surface guarded=%d expanded=%d brute-force maxerr=%.12g\n",guarded,expanded,worst);RuntimeSpectralSurfaceMap_Free(m);
    }
    {
        RuntimeSpectralPencil p={.receiver_radius_m=.01};p.map.adaptive_scattered=true;
        RuntimeScene3D scene={0};RuntimeSpectralMapStats stats={0};double invalid[]={NAN,-.1,.001,1};
        for(int i=0;i<4;i++){p.map.scattered_support_m=invalid[i];assert(!RuntimeSpectralSurfaceMap_Build(&p,&scene,&stats));}
    }
    puts("PASS wide surface: coarse-index/scalar parity, normalized area kernels, unscattered detail and coincident-surface guard");
}
