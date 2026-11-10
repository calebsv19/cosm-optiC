#include <assert.h>
#include <stdio.h>
#include "l3_json_faults.h"
#include "../src/editor/scene_editor_render_control.c"
int main(int argc,char **argv){assert(argc==2);snprintf(render.receipt,sizeof(render.receipt),"%s/receipt.json",argv[1]);strcpy(render.id,"held");strcpy(render.scene,"/l3/current.json");strcpy(render.state,"completed");render.verified=true;
 for(int variant=0;variant<2;variant++){l3_json_fail=0;l3_json_index=0;if(variant){options.integrator=RAY_TRACING_3D_INTEGRATOR_DISNEY_V3_SPECTRAL;options.deposition=EDITOR_SPECTRAL_SURFACE_VOLUME;options.sample_sequence=17;render.sampling.enabled=true;}
 assert(persist());int positions=l3_json_index;json_object *prior=json_object_from_file(render.receipt);assert(prior);
 for(int n=1;n<=positions;n++){l3_json_index=0;l3_json_fail=n;bool ok=persist();json_object *after=json_object_from_file(render.receipt);printf("receipt fault %d/%d ok=%d\n",n,positions,ok);fflush(stdout);assert(!ok);assert(after && json_object_equal(prior,after));json_object_put(after);}
 l3_json_fail=0;assert(persist());json_object_put(prior);
 l3_json_index=0;json_object *status=render_read_json(options,"held",false);assert(status);positions=l3_json_index;json_object_put(status);for(int n=1;n<=positions;n++){l3_json_index=0;l3_json_fail=n;assert(!render_read_json(options,"held",false));}
 }puts("PASS every render receipt JSON allocation/add refusal preserves prior durable receipt");return 0;}
