/* Homogeneous segment decomposition; no geometry, UI or photon reconstruction. */
#include "runtime_spectral_surface_path_internal.h"
SpectralMediumSplit spectral_medium_split(double sa,double ss,double length,double u) {
    double st=sa+ss;
    if(!(st>0) || !(length>0))return (SpectralMediumSplit){.ballistic=1};
    double tau=st*length,mass=-expm1(-tau),tr=exp(-tau);
    if(!(mass>0) || !(ss>0))return (SpectralMediumSplit){.ballistic=tr};
    double distance=-log1p(-u*mass)/st;
    return (SpectralMediumSplit){tr,(ss/st)*mass,distance,st*exp(-st*distance)/mass};
}
/* Only survival allocation changes: E[1(U<p)*beta/p] remains beta.
 * No collision probability, source PDF, spectral weight or map term changes. */
double spectral_path_roulette_survival(const RuntimeSpectralSurfacePathConfig *p,double beta,int volume_events) {
    return p->fog_survival && volume_events>0 ? .95 : fmin(.95,beta);
}
int spectral_path_max_branches(const RuntimeSpectralSurfacePathConfig *p) {
    return 1+(p->glass_split_first?1:0)+(p->glass_split_bounded?3:p->glass_split_camera_exit?1:0)+(p->medium_split_first?1:0);
}
void spectral_path_signal_accumulate(PathSignal *to,const PathSignal *from) {
    to->direct+=from->direct;to->indirect+=from->indirect;to->volume+=from->volume;
    to->caustic+=from->caustic;to->caustic_bounce+=from->caustic_bounce;
    to->volume_caustic+=from->volume_caustic;to->replaced_source+=from->replaced_source;
    for(int k=0;k<SPECTRAL_PATH_DEPTHS;++k)to->depth[k]+=from->depth[k];
    for(int k=0;k<4;++k)to->volume_order[k]+=from->volume_order[k];
    for(int k=0;k<5;++k)to->nonscattered_indirect[k]+=from->nonscattered_indirect[k];
    to->failed|=from->failed;
}
