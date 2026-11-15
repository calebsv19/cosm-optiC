#include "runtime_spectral_surface_path_internal.h"

/* Repeated-scatter proposal, in solid-angle measure throughout. A spherical
 * cap encloses the area rectangle; near its plane a hemisphere is the bound.
 * Directions missing the rectangle are valid null light samples. Never reject
 * and resample them: that would require a different normalization. */
double spectral_volume_phase(double g, double cosine) {
    double denominator = 1 + g * g - 2 * g * cosine;
    return (1 - g * g) / (4 * M_PI * denominator * sqrt(denominator));
}

static Vec3 sample_phase(double g, Vec3 incoming, double u, double v) {
    double cosine = 1 - 2 * u;
    if (fabs(g) > 1e-5) {
        double t = (1 - g * g) / (1 - g + 2 * g * u);
        cosine = (1 + g * g - t * t) / (2 * g);
    }
    return spectral_path_direction(incoming, fmax(-1, fmin(1, cosine)), 2 * M_PI * v);
}


SpectralVolumeProposal spectral_volume_proposal(const RuntimeSpectralSurfacePath *context,Vec3 at,int order) {
    const RuntimeSpectralSurfacePathConfig *p=&context->profile->surface_path;
    SpectralVolumeProposal q={0};
    if((!p->volume_light_mis && !p->volume_light_only && !p->volume_rectangle_mis) || order<2 || p->native_sources || p->sky ||
       !(vec3_dot(vec3_sub(at,p->origin),p->normal)>1e-7))return q;
    Vec3 delta=vec3_sub(p->origin,at);
    double d2=vec3_dot(delta,delta),r2=.25*(p->width*p->width+p->height*p->height);
    q.axis=vec3_scale(p->normal,-1);q.one_minus_cosine=1;
    if(d2>r2) {
        double ratio=r2/d2;
        /* Stable 1-sqrt(1-ratio) for distant small emitters. */
        q.one_minus_cosine=ratio/(1+sqrt(1-ratio));
        q.axis=vec3_scale(delta,1/sqrt(d2));
    }
    if(!(q.one_minus_cosine>0))return (SpectralVolumeProposal){0};
    q.light_only=p->volume_light_only;
    q.pdf=1/(2*M_PI*q.one_minus_cosine);q.active=isfinite(q.pdf);
    if(q.active && p->volume_rectangle_mis)spectral_volume_rectangle_init(&q,p,at);
    return q;
}
double spectral_volume_light_pdf(const SpectralVolumeProposal *q,Vec3 direction) {
    if(q->active && q->rectangle) {
        double cosine=-vec3_dot(q->normal,direction);
        if(!(cosine>0))return 0;
        double t=q->h/cosine,x=t*vec3_dot(q->u_axis,direction),y=t*vec3_dot(q->v_axis,direction);
        /* Roundoff tolerance at rectangle edges, not a wider proposal. */
        double eps=1e-12*fmax(1,fmax(fabs(q->x0)+fabs(q->x1),fabs(q->y0)+fabs(q->y1)));
        return x>=q->x0-eps && x<=q->x1+eps && y>=q->y0-eps && y<=q->y1+eps ? q->pdf : 0;
    }
    return q->active && vec3_dot(q->axis,direction)>=1-q->one_minus_cosine ? q->pdf : 0;
}
double spectral_volume_direction_pdf(double g,Vec3 incoming,Vec3 direction,const SpectralVolumeProposal *q) {
    double phase=spectral_volume_phase(g,vec3_dot(incoming,direction));
    return q->active && !q->light_only ? .75*phase+.25*spectral_volume_light_pdf(q,direction) : phase;
}
Vec3 spectral_volume_sample_direction(double g,Vec3 incoming,const SpectralVolumeProposal *q,
                                     double u,double v,double *pdf,double *value) {
    Vec3 direction;
    /* light_only changes NEE and its competing emitter-hit PDF, but preserves
     * the phase continuation and its random dimensions exactly. */
    if(!q->active || q->light_only)direction=sample_phase(g,incoming,u,v);
    else if(u<.25)direction=spectral_volume_sample_proposal(q,u/.25,v);
    else direction=sample_phase(g,incoming,(u-.25)/.75,v);
    *value=spectral_volume_phase(g,vec3_dot(incoming,direction));
    *pdf=spectral_volume_direction_pdf(g,incoming,direction,q);
    return direction;
}
SourceSample spectral_volume_sample_light(const RuntimeSpectralSurfacePath *context,Vec3 at,
                                          const SpectralVolumeProposal *q,PathRandom *rng) {
    double u=spectral_path_random(rng),v=spectral_path_random(rng);
    SourceSample s={.direction=spectral_volume_sample_proposal(q,u,v),
                    .pdf=q->pdf,.weight=1,.selection_pdf=1,.stratum_pdf=1,.source_index=-1};
    s.distance=spectral_path_source_intersection(context,RuntimeRay3D_Make(at,s.direction));
    s.valid=isfinite(s.distance);
    return s;
}
