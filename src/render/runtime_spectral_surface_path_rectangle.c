#include "runtime_spectral_surface_path_internal.h"

/* Urena, Fajardo and King (EGSR 2013), An Area-Preserving Parametrization for
 * Spherical Rectangles. Solid-angle inversion, with a geometry-only cap fallback
 * for tiny solid angles or nearly coplanar vertices. The fallback retains its
 * own normalized cap PDF in BOTH techniques; no per-sample rejection/resampling.
 * See docs/spectral_estimator.md for qualification and reference. */
static double corner(double x,double y,double h) {
    return atan2(x*y,h*sqrt(x*x+y*y+h*h));
}
static double interior(Vec3 a,Vec3 b) {
    return acos(fmax(-1,fmin(1,-vec3_dot(a,b))));
}
bool spectral_volume_rectangle_init(SpectralVolumeProposal *q,const RuntimeSpectralSurfacePathConfig *p,Vec3 at) {
    Vec3 d=vec3_sub(p->origin,at);
    double x=vec3_dot(d,p->axis_u),y=vec3_dot(d,p->axis_v),h=-vec3_dot(d,p->normal);
    double x0=x-p->width*.5,x1=x+p->width*.5,y0=y-p->height*.5,y1=y+p->height*.5;
    double scale=.5*hypot(p->width,p->height);
    if(!(h>1e-5*scale))return false;
    double omega=corner(x1,y1,h)-corner(x0,y1,h)-corner(x1,y0,h)+corner(x0,y0,h);
    if(!(omega>=1e-4 && omega<2*M_PI) || !isfinite(omega))return false;
    Vec3 v[4]={vec3(x0,y0,-h),vec3(x1,y0,-h),vec3(x1,y1,-h),vec3(x0,y1,-h)},n[4];
    for(int i=0;i<4;++i)n[i]=vec3_normalize(vec3_cross(v[i],v[(i+1)%4]));
    q->x0=x0;q->x1=x1;q->y0=y0;q->y1=y1;q->h=h;q->solid_angle=omega;
    q->b0=n[0].z;q->b1=n[2].z;q->angle_offset=interior(n[2],n[3])+interior(n[3],n[0]);
    q->u_axis=p->axis_u;q->v_axis=p->axis_v;q->normal=p->normal;
    q->pdf=1/omega;q->rectangle=true;return true;
}
Vec3 spectral_volume_sample_proposal(const SpectralVolumeProposal *q,double u,double v) {
    if(!q->rectangle)return spectral_path_direction(q->axis,1-u*q->one_minus_cosine,2*M_PI*v);
    double angle=u*q->solid_angle-q->angle_offset;
    double f=(cos(angle)*q->b0-q->b1)/sin(angle);
    double c=copysign(1/hypot(f,q->b0),f);
    c=fmax(-nextafter(1,0),fmin(nextafter(1,0),c));
    double x=q->h*c/sqrt((1-c)*(1+c));
    x=fmax(q->x0,fmin(q->x1,x));
    double d=hypot(x,q->h),h0=q->y0/hypot(d,q->y0),h1=q->y1/hypot(d,q->y1);
    double hv=h0+v*(h1-h0);
    hv=fmax(-nextafter(1,0),fmin(nextafter(1,0),hv));
    double y=hv*d/sqrt((1-hv)*(1+hv));
    y=fmax(q->y0,fmin(q->y1,y));
    return vec3_normalize(vec3_add(vec3_add(vec3_scale(q->u_axis,x),vec3_scale(q->v_axis,y)),vec3_scale(q->normal,-q->h)));
}
