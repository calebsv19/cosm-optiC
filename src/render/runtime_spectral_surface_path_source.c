#include "runtime_spectral_surface_path_internal.h"
#include "render/runtime_incident_light_3d.h"

Vec3 spectral_path_direction(Vec3 axis,double cosine,double phi) {
    Vec3 guide=fabs(axis.z)<.9 ? vec3(0,0,1) : vec3(0,1,0);
    Vec3 u=vec3_normalize(vec3_cross(guide,axis)),v=vec3_cross(axis,u);
    double sine=sqrt(fmax(0,1-cosine*cosine));
    return vec3_add(vec3_scale(axis,cosine),vec3_add(vec3_scale(u,sine*cos(phi)),vec3_scale(v,sine*sin(phi))));
}
double spectral_path_source_intersection(const RuntimeSpectralSurfacePath *context, Ray3D ray) {
    const RuntimeSpectralSurfacePathConfig *p = &context->profile->surface_path;
    if (p->native_sources) return INFINITY;
    double nd = vec3_dot(ray.direction, p->normal);
    if (p->sky || fabs(nd) < 1e-12) return INFINITY;
    double t = vec3_dot(vec3_sub(p->origin, ray.origin), p->normal) / nd;
    if (t <= 1e-7) return INFINITY;
    Vec3 offset = vec3_sub(vec3_add(ray.origin, vec3_scale(ray.direction, t)), p->origin);
    if (fabs(vec3_dot(offset, p->axis_u)) > p->width * .5 ||
        fabs(vec3_dot(offset, p->axis_v)) > p->height * .5) return INFINITY;
    return t;
}

double spectral_path_source_pdf(const RuntimeSpectralSurfacePath *context, Ray3D ray, double distance) {
    const RuntimeSpectralSurfacePathConfig *p = &context->profile->surface_path;
    if (p->native_sources) return 0;
    if (p->sky) {
        double limit = cos(p->sky_angle * M_PI / 180);
        return vec3_dot(ray.direction, p->sky_axis) >= limit ? 1 / (2 * M_PI * (1 - limit)) : 0;
    }
    double cosine = -vec3_dot(p->normal, ray.direction);
    if (!(cosine > 0) || !isfinite(distance)) return 0;
    return distance * distance / (cosine * p->width * p->height);
}

SourceSample spectral_path_sample_source(const RuntimeSpectralSurfacePath *context, Vec3 at, double nm, PathRandom *rng) {
    const RuntimeSpectralSurfacePathConfig *p = &context->profile->surface_path;
    SourceSample s = {.weight=1, .source_index=-1,.selection_pdf=1,.stratum_pdf=1};
    if (p->native_sources) {
        int index = (int)(spectral_path_random(rng) * context->source_count);
        const RuntimeLightSource3D *source = RuntimeLightSet3D_GetEnabled(&context->scene->lightSet, index);
        int count = RuntimeIncidentLight3D_SampleCount(source);
        if(source) {s.source_index=index;s.selection_pdf=1./context->source_count;}
        if (!count) return s;
        s.stratum_pdf=1./count;
        int stratum = (int)(spectral_path_random(rng) * count);
        RuntimeNative3DSamplingContext sampling = {.sampleSequence=(uint32_t)(spectral_path_random(rng)*4294967295.)};
        RuntimeIncidentLight3DSample sample;
        if (!RuntimeIncidentLight3D_Sample(source, at, stratum, &sampling, &sample)) return s;
        /* Native incident sampling already includes source geometry/radiometry.
         * This adapter is the only RGB -> spectral boundary. The uniform
         * wavelength proposal is compensated by the common film sampler. */
        double gain=fmax(sample.rgb_weight.x,fmax(sample.rgb_weight.y,sample.rgb_weight.z));
        s.direction=sample.direction; s.distance=sample.distance; s.pdf=1; s.delta=true;
        s.source_index=index;
        s.weight=gain>0 ? context->source_count*gain*
            RuntimeSpectral_RGBBasis(vec3_scale(sample.rgb_weight,1/gain),nm)/context->native_normalization : 0;
        s.valid=s.weight>0;
        return s;
    }
    double u = spectral_path_random(rng), v = spectral_path_random(rng);
    if (p->sky) {
        double cosine = 1 - u * (1 - cos(p->sky_angle * M_PI / 180));
        s.direction = spectral_path_direction(p->sky_axis, cosine, 2 * M_PI * v);
        s.distance = INFINITY;
    } else {
        Vec3 target = vec3_add(p->origin,
            vec3_add(vec3_scale(p->axis_u, (u - .5) * p->width),
                     vec3_scale(p->axis_v, (v - .5) * p->height)));
        Vec3 delta = vec3_sub(target, at);
        s.distance = vec3_length(delta);
        if (s.distance < 1e-7) return s;
        s.direction = vec3_scale(delta, 1 / s.distance);
    }
    s.pdf = spectral_path_source_pdf(context, RuntimeRay3D_Make(at, s.direction), s.distance);
    s.valid = s.pdf > 0;
    return s;
}
