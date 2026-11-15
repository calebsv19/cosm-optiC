#include "runtime_spectral_surface_path_internal.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_volume.h"
#include "app/ray_tracing_sha256.h"
#include "render/runtime_incident_light_3d.h"
#include "render/runtime_specular_bsdf_3d.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_scene_accel_3d_internal.h"
#include <float.h>
#include <math.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>

static double mis_weight(double a, double b) {
    if (!(a > 0)) return 0;
    double ratio = b / a;
    return 1 / (1 + ratio * ratio);
}

static bool medium_interval(const RuntimeSpectralSurfacePathConfig *p, Ray3D ray,
                            double distance, double *lo, double *hi) {
    *lo = 0;
    *hi = distance;
    double origin[] = {ray.origin.x, ray.origin.y, ray.origin.z};
    double dir[] = {ray.direction.x, ray.direction.y, ray.direction.z};
    double low[] = {p->medium_lo.x, p->medium_lo.y, p->medium_lo.z};
    double high[] = {p->medium_hi.x, p->medium_hi.y, p->medium_hi.z};
    for (int i = 0; i < 3; ++i) {
        if (fabs(dir[i]) < 1e-15) {
            if (origin[i] < low[i] || origin[i] >= high[i]) return false;
        } else {
            double a = (low[i] - origin[i]) / dir[i], b = (high[i] - origin[i]) / dir[i];
            *lo = fmax(*lo, fmin(a, b));
            *hi = fmin(*hi, fmax(a, b));
        }
    }
    return *lo < *hi;
}

static double transmission(const RuntimeSpectralSurfacePathConfig *p, Ray3D ray,
                           double distance, double sigma_t) {
    double a, b;
    return p->medium && medium_interval(p, ray, distance, &a, &b)
        ? exp(-sigma_t * (b - a)) : 1;
}

static double boundary_epsilon(Vec3 position) {
    double scale = fmax(1, fmax(fabs(position.x), fmax(fabs(position.y), fabs(position.z))));
    return 64 * DBL_EPSILON * scale;
}

static bool source_visible(const RuntimeScene3D *scene, Vec3 at, const SourceSample *s,
                           double minimum, int *blocked) {
    *blocked=-1;
    if (!s->valid) return false;
    HitInfo3D obstruction;
    Ray3D ray = RuntimeRay3D_Make(at, s->direction);
    double limit = isfinite(s->distance) ? s->distance - 1e-7
                                        : RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE;
    bool hit=RuntimeRay3D_TraceSceneFirstHit(scene, &ray, minimum, limit, &obstruction);
    if(hit)*blocked=obstruction.primitiveIndex;
    return !hit;
}

static void add_signal(PathSignal *s, double value, int depth, int volume) {
    if (volume) {
        s->volume += value;
        int group=volume==1?0:volume==2?1:volume<=4?2:3;
        s->volume_order[group]+=value;
    }
    else if (depth == 0) s->direct += value;
    else s->indirect += value;
    if (!volume && depth < SPECTRAL_PATH_DEPTHS) s->depth[depth] += value;
}

static bool glass_interface(const RuntimeSpectralPencil *profile,double nm,bool inside,
                            Vec3 normal,Vec3 direction,RuntimeDielectricTransport3D *event) {
    const RuntimeMaterialPayload3D material = {.hasGlassTransportOverride=true};
    double ior=RuntimeSpectralPencil_Index(profile,nm);
    double from=inside?ior:1,to=inside?1:ior;
    return RuntimeDielectricTransport3D_ResolveInterface(&material,normal,direction,from,to,event)
        && event->entering!=inside;
}

typedef enum {GLASS_ROULETTE,GLASS_SPLIT_REFLECT,GLASS_SPLIT_TRANSMIT} GlassBranch;
static bool glass_step(const RuntimeSpectralPencil *p,const HitInfo3D *hit,Vec3 normal,
    RuntimeDielectricTransport3D event,double nm,Ray3D *ray,PathRandom *rng,double *beta,bool *inside,
    int depth,int volume_events,GlassBranch branch,RuntimeSpectralSurfacePathReadout *stats,
    RuntimeSpectralSurfacePathReadout *trace) {
    double before = *beta;
    double tr = *inside ? exp(-p->surface_path.caustic_absorption * hit->t) : 1;
    *beta *= tr;
    bool reflected=event.totalInternalReflection || spectral_path_random(rng)<event.fresnel;
    if (branch!=GLASS_ROULETTE) {
        reflected=branch==GLASS_SPLIT_REFLECT;
        *beta*=reflected?event.fresnel:1-event.fresnel;
    }
    Vec3 next = reflected ? event.reflectionDir : event.refractionDir;
    if (reflected) {
        ++stats->glass_reflections;
        if (event.totalInternalReflection) ++stats->glass_tir;
    } else {
        ++stats->glass_transmissions;
        *inside = !*inside;
        double eta=event.etaFrom/event.etaTo;
        *beta *= eta*eta; /* Camera radiance transport across the interface. */
    }
    double epsilon = (vec3_dot(next,normal)<0 ? -1 : 1)*boundary_epsilon(hit->position);
    *ray = RuntimeRay3D_Make(vec3_add(hit->position,vec3_scale(normal,epsilon)),next);
    RuntimeSpectralPathEvent *e = spectral_path_event(trace,"dielectric",p->glass_id,
        branch!=GLASS_ROULETTE?(reflected?"split_reflection":"split_transmission"):
        event.totalInternalReflection?"total_internal_reflection":reflected?"fresnel_reflection":"fresnel_transmission",
        hit->position,next,nm,before,*beta,depth,volume_events);
    if (e) {
        e->normal=normal;e->distance=hit->t;e->reflectance=event.fresnel;e->transmittance=tr;
        e->bsdf_pdf=reflected?event.fresnel:1-event.fresnel;
        e->continuation_weight=before>0?*beta/before:0;
    }
    return reflected;
}

typedef struct {
    int depth,previous_depth,volume_events,event;
    double previous_pdf;
    bool inside_glass,delta_tail,after_diffuse,last_nondelta_volume;
    bool skip_medium,skip_gather;
    int glass_reflections,glass_transmissions;
} PathContinuation;
typedef struct { int medium,glass; } SplitBudget;

static PathSignal trace_path(const RuntimeSpectralSurfacePath *context, Ray3D ray, double nm,
    RuntimeSpectralMeasure measure,PathContinuation state,double initial_beta,SplitBudget *budget,
    PathRandom *rng, RuntimeSpectralSurfacePathReadout *stats, RuntimeSpectralSurfacePathReadout *trace) {
    const RuntimeSpectralSurfacePathConfig *p=&context->profile->surface_path;
    const RuntimeScene3D *scene=context->scene;
    PathSignal signal={0};
    double beta=initial_beta,previous_pdf=state.previous_pdf;
    int depth=state.depth,previous_depth=state.previous_depth,volume_events=state.volume_events;
    bool inside_glass=state.inside_glass,delta_tail=state.delta_tail,after_diffuse=state.after_diffuse,last_nondelta_volume=state.last_nondelta_volume;
    int gr=state.glass_reflections,gt=state.glass_transmissions;
    double sigma_a=p->medium ? RuntimeSpectralCurve_Value(&p->sigma_a,nm) : 0;
    double sigma_s=p->medium ? RuntimeSpectralCurve_Value(&p->sigma_s,nm) : 0;
    double sigma_t=sigma_a+sigma_s;
    ++stats->paths;
    for (int event=state.event;event<32;++event) {
        bool skip_medium=state.skip_medium && event==state.event;
        bool skip_gather=state.skip_gather && event==state.event;
        spectral_path_random_event(rng,(unsigned)event);
        HitInfo3D hit;
        /* A volume collision is not a surface hit. An exterior collision
         * can be arbitrarily close to the lens, and must see its first face. */
        bool near_boundary=delta_tail || (last_nondelta_volume && context->glass_primitive>=0);
        double minimum=near_boundary ? boundary_epsilon(ray.origin) : 1e-7;
        bool found=RuntimeRay3D_TraceSceneFirstHit(scene,&ray,minimum,RUNTIME_RAY_3D_UNBOUNDED_SCENE_DISTANCE,&hit);
        double source_t=spectral_path_source_intersection(context,ray);
        double distance=found ? hit.t : INFINITY;
        bool light_first=source_t<distance;
        distance=fmin(distance,source_t);
        bool scatter=false;
        Vec3 position=ray.origin;
        /* Integrate the mapped specular-to-medium source along this camera
         * segment. The analog walk below samples different paths that continue
         * through a real medium collision. Both stop at the next boundary. */
        if (!skip_gather && !inside_glass && context->volume_caustics && sigma_s>0) {
            double response;
            if (!RuntimeSpectralSurfaceMap_AreaVolumeResponse(context->caustic_map,context->photon_profile,
                                                            ray,distance,nm,measure,&response)) {
                signal.failed=true;return signal;
            }
            double value=beta*response;
            signal.volume+=value;
            signal.volume_caustic+=value;
            if (value>0) {
                RuntimeSpectralPathEvent *e=spectral_path_event(trace,"photon_volume_gather","","specular_to_fog",
                    ray.origin,ray.direction,nm,beta,beta,depth,volume_events);
                if(e) {e->contribution=value;snprintf(e->component,sizeof(e->component),"volume_caustic");}
            }
        }
        if (!skip_medium && !inside_glass && p->medium && sigma_t>0) {
            double a,b;
            if (medium_interval(p,ray,distance,&a,&b)) {
                if (sigma_s==0) {
                    double before=beta,tr=exp(-sigma_a*(b-a));
                    beta*=tr;
                    RuntimeSpectralPathEvent *e=spectral_path_event(trace,"transmission","","beer_lambert",ray.origin,ray.direction,nm,before,beta,depth,volume_events);
                    if(e) {e->distance=b-a;e->transmittance=tr;e->absorption_per_m=sigma_a;}
                } else if (budget->medium>0 && (sigma_s/sigma_t)*(-expm1(-sigma_t*(b-a)))>0) {
                    --budget->medium;
                    SpectralMediumSplit split=spectral_medium_split(sigma_a,sigma_s,b-a,spectral_path_random(rng));
                    RuntimeSpectralPathEvent *e=spectral_path_event(trace,"medium_split","","ballistic_and_scattered",ray.origin,ray.direction,nm,beta,beta*split.scattered,depth,volume_events);
                    if(e) {e->distance=b-a;e->transmittance=split.ballistic;e->continuation_weight=split.scattered;e->bsdf_pdf=split.conditional_pdf;e->absorption_per_m=sigma_a;e->scattering_per_m=sigma_s;}
                    if(split.ballistic>0) {
                        PathRandom child=*rng;spectral_path_random_fork(&child,UINT64_C(0x8ebc6af09c88c6e3));
                        PathContinuation next={depth,previous_depth,volume_events,event,previous_pdf,inside_glass,delta_tail,after_diffuse,last_nondelta_volume,true,true,gr,gt};
                        ++stats->medium_splits;
                        PathSignal ballistic=trace_path(context,ray,nm,measure,next,beta*split.ballistic,budget,&child,stats,trace);
                        spectral_path_signal_accumulate(&signal,&ballistic);
                        if(signal.failed)return signal;
                    }
                    beta*=split.scattered;
                    position=vec3_add(ray.origin,vec3_scale(ray.direction,a+split.distance));
                    scatter=true;++volume_events;++stats->volume_events;
                } else {
                    double flight=-log(fmax(1e-15,1-spectral_path_random(rng)))/sigma_t;
                    if(flight<b-a) {
                        position=vec3_add(ray.origin,vec3_scale(ray.direction,a+flight));
                        if(spectral_path_random(rng)>=sigma_s/sigma_t) {
                            ++stats->absorbed;++stats->medium_absorbed;
                            spectral_path_event(trace,"terminated","","medium_absorption",position,ray.direction,nm,beta,0,depth,volume_events);
                            return signal;
                        }
                        scatter=true;++volume_events;++stats->volume_events;
                    }
                }
            }
        }
        if (inside_glass && (light_first || !found || hit.primitiveIndex != context->glass_primitive)) {
            signal.failed=true;return signal;
        }
        if (!scatter && found && !light_first && hit.primitiveIndex==context->glass_primitive) {
            ++stats->surface_events;
            if (depth+1>=p->max_depth) {
                ++stats->limited;++stats->depth_limited;
                spectral_path_event(trace,"terminated",context->profile->glass_id,"surface_depth_limit",
                                    hit.position,ray.direction,nm,beta,0,depth,volume_events);
                return signal;
            }
            Vec3 normal=HitInfo3D_BoundaryNormal(&hit);
            RuntimeDielectricTransport3D crossing;
            if (!glass_interface(context->profile,nm,inside_glass,normal,ray.direction,&crossing)) {
                signal.failed=true;return signal;
            }
            GlassBranch branch=GLASS_ROULETTE;
            if (p->glass_split_first && event==0 && crossing.hasRefraction &&
                !crossing.totalInternalReflection && crossing.fresnel>0 && crossing.fresnel<1) {
                Ray3D reflected_ray=ray;
                PathRandom reflected_rng=*rng;
                bool reflected_inside=false;
                double reflected_beta=beta;
                glass_step(context->profile,&hit,normal,crossing,nm,&reflected_ray,&reflected_rng,
                           &reflected_beta,&reflected_inside,depth,volume_events,GLASS_SPLIT_REFLECT,stats,trace);
                ++stats->first_interface_splits;
                spectral_path_random_fork(&reflected_rng,UINT64_C(0xe7037ed1a0b428db));
                PathSignal reflected=trace_path(context,reflected_ray,nm,measure,(PathContinuation){.depth=1,.event=1,.delta_tail=true,.glass_reflections=1},reflected_beta,budget,
                                               &reflected_rng,stats,trace);
                reflected.volume+=signal.volume;
                reflected.volume_caustic+=signal.volume_caustic;
                for(int k=0;k<4;++k)reflected.volume_order[k]+=signal.volume_order[k];
                signal=reflected;
                if (signal.failed) return signal;
                branch=GLASS_SPLIT_TRANSMIT;
            } else if (budget->glass>0 &&
                       /* Reserve the exit split for camera-to-glass visibility.
                        * Diffuse/scattered paths retain ordinary Fresnel sampling;
                        * their map ownership and estimator weights do not change. */
                       (!p->glass_split_camera_exit || (inside_glass && !after_diffuse && volume_events==0)) &&
                       crossing.hasRefraction &&
                       !crossing.totalInternalReflection && crossing.fresnel>0 && crossing.fresnel<1) {
                --budget->glass;
                Ray3D reflected_ray=ray;PathRandom child=*rng;
                bool reflected_inside=inside_glass;double reflected_beta=beta;
                glass_step(context->profile,&hit,normal,crossing,nm,&reflected_ray,&child,&reflected_beta,
                    &reflected_inside,depth,volume_events,GLASS_SPLIT_REFLECT,stats,trace);
                spectral_path_random_fork(&child,UINT64_C(0x589965cc75374cc3)^((uint64_t)event<<32));
                PathContinuation next={depth+1,depth,volume_events,event+1,previous_pdf,reflected_inside,true,after_diffuse,last_nondelta_volume,false,false,gr+1,gt};
                ++stats->internal_interface_splits;
                PathSignal reflected=trace_path(context,reflected_ray,nm,measure,next,reflected_beta,budget,&child,stats,trace);
                spectral_path_signal_accumulate(&signal,&reflected);
                if(signal.failed)return signal;
                branch=GLASS_SPLIT_TRANSMIT;
            }
            bool reflected=glass_step(context->profile,&hit,normal,crossing,nm,&ray,rng,&beta,&inside_glass,
                       depth,volume_events,branch,stats,trace);
            if(reflected)++gr;else ++gt;
            previous_depth=depth++;
            delta_tail=true;
        } else if(scatter) {
            SpectralVolumeProposal proposal=spectral_volume_proposal(context,position,volume_events);
            SourceSample light=proposal.active ? spectral_volume_sample_light(context,position,&proposal,rng) :
                spectral_path_sample_source(context,position,nm,rng);
            int blocked=-1;
            double shadow_minimum=context->glass_primitive>=0 ? boundary_epsilon(position) : 1e-7;
            bool visible=source_visible(scene,position,&light,shadow_minimum,&blocked);
            const char *light_id=light.source_index>=0 ? RuntimeLightSet3D_GetEnabled(&scene->lightSet,light.source_index)->id : p->sky?"sky":"area";
            RuntimeSpectralPathEvent *e=spectral_path_event(trace,"light_sample",light_id,visible?"visible":light.valid?"occluded":"zero_source",position,light.direction,nm,beta,beta,depth,volume_events);
            if(e) {
                e->integrated_light=light.delta;e->distant_source=!isfinite(light.distance);
                e->distance=isfinite(light.distance)?light.distance:0;e->selection_pdf=light.selection_pdf;e->stratum_pdf=light.stratum_pdf;
                if(blocked>=0)snprintf(e->occluder_id,sizeof(e->occluder_id),"%s",scene->primitives[blocked].source.objectId);
            }
            double nee_value=0;
            if(visible) {
                double f=spectral_volume_phase(p->g,vec3_dot(ray.direction,light.direction));
                double q=spectral_volume_direction_pdf(p->g,ray.direction,light.direction,&proposal);
                double tr=transmission(p,RuntimeRay3D_Make(position,light.direction),light.distance,sigma_t);
                double weight=light.delta ? 1 : mis_weight(light.pdf,q);
                double value=beta*tr*f/light.pdf*weight*light.weight;
                nee_value=value;
                add_signal(&signal,value,depth,volume_events);
                if(e) {e->bsdf_fcos=f;e->bsdf_pdf=q;e->light_pdf=light.pdf;e->mis_weight=weight;e->contribution=value;e->source_weight=light.weight;e->transmittance=tr;snprintf(e->component,sizeof(e->component),"%s",volume_events?"volume":depth?"surface_indirect":"direct");}
            }
            spectral_volume_diagnostic(p,stats,position,volume_events,proposal.rectangle?2:proposal.active?1:0,visible?2:light.valid?1:0,nee_value);
            double u=spectral_path_random(rng),v=spectral_path_random(rng);
            double before=beta,f;
            Vec3 next=spectral_volume_sample_direction(p->g,ray.direction,&proposal,u,v,&previous_pdf,&f);
            if(proposal.active)beta*=f/previous_pdf;
            previous_depth=depth;
            e=spectral_path_event(trace,"volume","",proposal.active?(proposal.rectangle?"rectangle_mis_scatter":proposal.light_only?"light_only_scatter":"light_mis_scatter"):"hg_scatter",position,next,nm,before,beta,depth,volume_events);
            if(e) {e->bsdf_fcos=f;e->bsdf_pdf=previous_pdf;
                e->light_pdf=spectral_volume_light_pdf(&proposal,next);
                e->continuation_weight=proposal.active?f/previous_pdf:1;
                e->absorption_per_m=sigma_a;e->scattering_per_m=sigma_s;}
            ray=RuntimeRay3D_Make(position,next);
            delta_tail=false;
            last_nondelta_volume=true;
        } else if(light_first || !found) {
            double pdf=spectral_path_source_pdf(context,ray,source_t),value=0,weight=0;
            /* The competing light technique belongs to the last non-delta vertex.
             * Surface vertices retain their area proposal; delta chains use weight one. */
            if(last_nondelta_volume && !delta_tail) {
                SpectralVolumeProposal proposal=spectral_volume_proposal(context,ray.origin,volume_events);
                if(proposal.active)pdf=spectral_volume_light_pdf(&proposal,ray.direction);
            }
            bool owned_by_map=context->caustic_map && delta_tail &&
                (last_nondelta_volume ? context->volume_caustics : after_diffuse);
            if(((p->sky && !found)||light_first) && pdf>0) {
                if (owned_by_map) {
                    /* The last non-delta interaction is supplied by its map:
                     * either a matte surface or the first medium scatter. */
                    ++stats->replaced_source_hits;
                    if(last_nondelta_volume)++stats->replaced_volume_source_hits;
                    signal.replaced_source+=beta;
                } else {
                    weight=event==0 || delta_tail ? 1 : mis_weight(previous_pdf,pdf);
                    value=beta*weight;
                    spectral_nonscattered_add(&signal,value,previous_depth,volume_events,after_diffuse,gr,gt);
                    if(last_nondelta_volume && !delta_tail)
                        spectral_volume_diagnostic(p,stats,ray.origin,volume_events,false,3,value);
                    add_signal(&signal,value,previous_depth,volume_events);
                }
                ++stats->source_hit;
            } else ++stats->dark_escape;
            RuntimeSpectralPathEvent *e=spectral_path_event(trace,"terminated",pdf>0?(p->sky?"sky":"area"):"",pdf>0?(owned_by_map?
                "source_hit_owned_by_map":"source_hit"):"dark_escape",ray.origin,ray.direction,nm,beta,0,depth,volume_events);
            if(e) {e->bsdf_pdf=previous_pdf;e->light_pdf=pdf;e->mis_weight=weight;e->contribution=value;snprintf(e->component,sizeof(e->component),"%s",volume_events?"volume":previous_depth?"surface_indirect":"direct");}
            ++stats->escaped;return signal;
        } else {
            PathMaterial material;
            if(!spectral_path_material(context,&hit,nm,&material)) {
                signal.failed=true;
                spectral_path_event(trace,"terminated",scene->primitives[hit.primitiveIndex].source.objectId,"unsupported_material",hit.position,ray.direction,nm,beta,0,depth,volume_events);
                return signal;
            }
            const char *id=scene->primitives[hit.primitiveIndex].source.objectId;
            Vec3 normal=HitInfo3D_BoundaryNormal(&hit),view=vec3_scale(ray.direction,-1);
            bool front=vec3_dot(normal,view)>0;
            if(!front) {
                if(!material.two_sided) {
                    ++stats->absorbed;++stats->backface;
                    spectral_path_event(trace,"terminated",id,"one_sided_backface",hit.position,ray.direction,nm,beta,0,depth,volume_events);
                    return signal;
                }
                normal=vec3_scale(normal,-1);
            }
            hit.normal=hit.geometricNormal=hit.shadingNormal=normal;hit.hasReflectionNormalField=false;
            ++stats->surface_events;
            if (context->caustic_map && material.rho>0) {
                RuntimeSpectralPathReadout query = {.position=hit.position,.primitive=hit.primitiveIndex,
                                                    .receiver_front=front};
                unsigned long long candidates=0;
                double response=RuntimeSpectralSurfaceMap_AreaResponse(context->caustic_map,
                    context->photon_profile,scene,&query,nm,measure,&candidates);
                double value=beta*material.rho*response;
                if (after_diffuse) signal.caustic_bounce+=value;
                else signal.caustic+=value;
                if (value>0) {
                    RuntimeSpectralPathEvent *mapped=spectral_path_event(trace,"photon_gather",id,
                        "specular_light_deposition",hit.position,ray.direction,nm,beta,beta,depth,volume_events);
                    if (mapped) {
                        mapped->reflectance=material.rho;mapped->contribution=value;
                        snprintf(mapped->component,sizeof(mapped->component),"%s",
                                 after_diffuse?"caustic_bounce":"caustic");
                    }
                }
            }
            position=vec3_add(hit.position,vec3_scale(normal,1e-7));
            /* A surface vertex retains its area/sky/native light proposal even
             * after earlier fog collisions. Its BSDF-hit MIS uses that same PDF. */
            SourceSample light=spectral_path_sample_source(context,position,nm,rng);
            int blocked=-1;
            bool visible=source_visible(scene,position,&light,1e-7,&blocked);
            const char *light_id=light.source_index>=0 ? RuntimeLightSet3D_GetEnabled(&scene->lightSet,light.source_index)->id : p->sky?"sky":"area";
            RuntimeSpectralPathEvent *e=spectral_path_event(trace,"light_sample",light_id,visible?"visible":light.valid?"occluded":"zero_source",position,light.direction,nm,beta,beta,depth,volume_events);
            if(e) {
                e->normal=normal;
                e->integrated_light=light.delta;e->distant_source=!isfinite(light.distance);
                e->distance=isfinite(light.distance)?light.distance:0;e->selection_pdf=light.selection_pdf;e->stratum_pdf=light.stratum_pdf;
                if(blocked>=0)snprintf(e->occluder_id,sizeof(e->occluder_id),"%s",scene->primitives[blocked].source.objectId);
            }
            if(visible) {
                spectral_surface_pdf_audit(context,stats,position,&light,volume_events);
                double pdf=0,fcos=spectral_path_material_eval(&material,&hit,view,light.direction,&pdf);
                double tr=transmission(p,RuntimeRay3D_Make(position,light.direction),light.distance,sigma_t);
                double weight=depth+1==p->max_depth || light.delta ? 1 : mis_weight(light.pdf,pdf);
                double value=beta*tr*fcos/light.pdf*weight*light.weight;
                spectral_nonscattered_add(&signal,value,depth,volume_events,true,gr,gt);
                add_signal(&signal,value,depth,volume_events);
                if(e) {e->reflectance=material.rho;e->bsdf_fcos=fcos;e->bsdf_pdf=pdf;e->light_pdf=light.pdf;e->mis_weight=weight;e->contribution=value;e->source_weight=light.weight;e->transmittance=tr;snprintf(e->component,sizeof(e->component),"%s",volume_events?"volume":depth?"surface_indirect":"direct");}
            }
            if(depth+1>=p->max_depth) {
                ++stats->limited;++stats->depth_limited;
                spectral_path_event(trace,"terminated",id,"surface_depth_limit",hit.position,ray.direction,nm,beta,0,depth,volume_events);
                return signal;
            }
            previous_depth=depth++;
            delta_tail=false;
            after_diffuse=true;
            last_nondelta_volume=false;
            Vec3 next;double pdf,weight;
            if(!spectral_path_material_sample(&material,&hit,view,rng,&next,&pdf,&weight)) {
                ++stats->absorbed;++stats->bsdf_rejected;
                spectral_path_event(trace,"terminated",id,"bsdf_rejected",hit.position,ray.direction,nm,beta,0,depth,volume_events);
                return signal;
            }
            double before=beta;beta*=weight;previous_pdf=pdf;
            e=spectral_path_event(trace,"surface",id,"elastic_reflection",hit.position,next,nm,before,beta,depth,volume_events);
            if(e) {e->reflectance=material.rho;e->bsdf_pdf=pdf;e->distance=hit.t;e->normal=normal;e->continuation_weight=weight;}
            ray=RuntimeRay3D_Make(position,next);
        }
        if(!(beta>0)) {
            ++stats->absorbed;++stats->surface_absorbed;
            spectral_path_event(trace,"terminated","","zero_throughput",ray.origin,ray.direction,nm,beta,0,depth,volume_events);
            return signal;
        }
        /* Keep low-weight split specular chains at their actual weights.
         * Roulette here would repeatedly boost surviving rare source hits.
         * The existing event/depth caps still bound every continuation. */
        if(event>=3 && (p->glass_split_bounded || p->glass_split_camera_exit) && delta_tail)++stats->specular_roulette_skipped;
        if(event>=3 && !((p->glass_split_bounded || p->glass_split_camera_exit) && delta_tail)) {
            double survival=spectral_path_roulette_survival(p,beta,volume_events),before=beta;
            bool survive=spectral_path_random(rng)<survival;
            if(p->fog_survival && volume_events>0) {
                ++stats->fog_roulette_events;
                if(survive)++stats->fog_roulette_survived;
            }
            RuntimeSpectralPathEvent *e=spectral_path_event(trace,"roulette","",survive?"survived":"terminated",ray.origin,ray.direction,nm,before,survive?beta/survival:0,depth,volume_events);
            if(e)e->survival=survival;
            if(!survive) {++stats->roulette;return signal;}
            beta/=survival;
        }
    }
    ++stats->limited;++stats->event_limited;
    spectral_path_event(trace,"terminated","","path_event_limit",ray.origin,ray.direction,nm,beta,0,depth,volume_events);
    return signal;
}

PathSignal spectral_path_trace(const RuntimeSpectralSurfacePath *context,Ray3D ray,double nm,
    RuntimeSpectralMeasure measure,PathRandom *rng,RuntimeSpectralSurfacePathReadout *stats,
    RuntimeSpectralSurfacePathReadout *trace) {
    const RuntimeSpectralSurfacePathConfig *p=&context->profile->surface_path;
    SplitBudget budget={p->medium_split_first?1:0,p->glass_split_bounded?3:p->glass_split_camera_exit?1:0};
    spectral_path_event(trace,"camera","","launch",ray.origin,ray.direction,nm,1,1,0,0);
    return trace_path(context,ray,nm,measure,(PathContinuation){0},1,&budget,rng,stats,trace);
}
