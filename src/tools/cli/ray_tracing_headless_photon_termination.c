/* Optional preparation diagnostics: constant counters and bounded streamed paths. */
#include "tools/ray_tracing_headless_photon_termination.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>

enum { TERMINATION_COUNT = RUNTIME_CAUSTIC_PHOTON_SCENE_TERMINATION_TRACE_ERROR + 1,
       MAX_UNMATCHED_EXPORTS = 64 };
static FILE* unmatched;
static char aggregate_path[4096];
static uint64_t counts[TERMINATION_COUNT], total, unmatched_count, exported;
static int requested_depth;
static bool overwrite, failed;

static void u(json_object* o,const char* k,uint64_t v) {
    json_object_object_add(o,k,json_object_new_uint64(v));
}
static bool emit(FILE* f,json_object* o) {
    bool ok=fprintf(f,"%s\n",json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN))>=0 && !ferror(f);
    json_object_put(o);return ok;
}
bool ray_tracing_headless_photon_termination_begin(const RayTracingAgentRenderRequest* r) {
    char path[4096];
    if (!r || unmatched) return false;
    if (snprintf(path,sizeof(path),"%s/photon_unmatched_paths_%04d.jsonl",r->output_root,r->start_frame)>=(int)sizeof(path) ||
        snprintf(aggregate_path,sizeof(aggregate_path),"%s/photon_termination_aggregate_%04d.json",r->output_root,r->start_frame)>=(int)sizeof(aggregate_path)) return false;
    unmatched=fopen(path,r->overwrite?"w":"wx");if (!unmatched) return false;
    memset(counts,0,sizeof(counts));total=unmatched_count=exported=0;
    requested_depth=r->caustic_photon_integration_settings.maxPathDepth;
    overwrite=r->overwrite;failed=false;return true;
}
void ray_tracing_headless_photon_termination_observe(const RuntimeCausticPhotonSample3D* sample,
                                                   const RuntimeCausticPhotonSceneTrace3D* t) {
    if (!unmatched || failed) return;
    total++;
    if ((unsigned)t->readback.termination<TERMINATION_COUNT) counts[t->readback.termination]++;
    uint64_t enters=0,exits=0;
    for (uint32_t i=0;i<t->readback.hitEventCount;i++) {
        const RuntimeCausticPhotonMediumTransition3D* m=&t->hitEvents[i].mediumTransition;
        if (m->succeeded && m->stackChanged) {
            enters+=m->reason==RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_ENTER_PUSHED;
            exits+=m->reason==RUNTIME_CAUSTIC_PHOTON_MEDIUM_TRANSITION_EXIT_POPPED;
        }
    }
    if (!t->readback.succeeded || !enters || enters==exits) return;
    unmatched_count++;if (exported==MAX_UNMATCHED_EXPORTS) return;
    json_object* o=json_object_new_object();json_object* events=json_object_new_array();
    json_object_object_add(o,"schema",json_object_new_string("ray_tracing_photon_unmatched_path_v1"));
    u(o,"photon_id",sample->photonId);u(o,"entry_count",enters);u(o,"exit_count",exits);
    json_object_object_add(o,"termination",json_object_new_string(RuntimeCausticPhotonSceneTermination3D_Label(t->readback.termination)));
    u(o,"reject_reason",t->trace.finalState.rejectReason);
    for (uint32_t i=0;i<t->readback.hitEventCount;i++) {
        const RuntimeCausticPhotonSceneHitEvent3D* h=&t->hitEvents[i];json_object* e=json_object_new_object();
        u(e,"depth",h->depth);u(e,"scene_object_index",h->hit.sceneObjectIndex);
        json_object_object_add(e,"lobe",json_object_new_string(RuntimeCausticPhotonBsdfLobe3D_Label(h->bsdfSelection.lobe)));
        json_object_object_add(e,"transition",json_object_new_string(RuntimeCausticPhotonMediumTransitionReason3D_Label(h->mediumTransition.reason)));
        json_object_object_add(e,"termination",json_object_new_string(RuntimeCausticPhotonSceneTermination3D_Label(h->termination)));
        u(e,"medium_depth_after",h->mediumTransition.depthAfter);json_object_array_add(events,e);
    }
    json_object_object_add(o,"events",events);if (!emit(unmatched,o)) failed=true;exported++;
}
bool ray_tracing_headless_photon_termination_end(void) {
    if (!unmatched) return true;
    if (fclose(unmatched)!=0) failed=true;unmatched=NULL;
    FILE* f=fopen(aggregate_path,overwrite?"w":"wx");if (!f) return false;
    json_object* o=json_object_new_object();json_object* histogram=json_object_new_object();
    json_object_object_add(o,"schema",json_object_new_string("ray_tracing_photon_termination_aggregate_v1"));
    u(o,"observed_paths",total);u(o,"requested_depth",requested_depth);
    u(o,"event_capacity",RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS);
    u(o,"effective_depth",requested_depth<RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS?
      requested_depth:RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS);
    u(o,"unmatched_full_paths",unmatched_count);u(o,"exported_unmatched_paths",exported);
    u(o,"unmatched_exports_omitted",unmatched_count-exported);
    for (int i=0;i<TERMINATION_COUNT;i++) u(histogram,RuntimeCausticPhotonSceneTermination3D_Label(i),counts[i]);
    json_object_object_add(o,"terminations",histogram);if (!emit(f,o)) failed=true;
    if (fclose(f)!=0) failed=true;return !failed;
}
