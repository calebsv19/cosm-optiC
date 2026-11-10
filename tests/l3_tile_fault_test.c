/* Faults at the actual scheduler boundary; the renderer and worker pool are real. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "core_workers.h"
#include "core_queue.h"
static int allocation_index, fail_allocation, outstanding, submit_index, fail_submit;
static int fail_queue, fail_pool, fail_completion, fail_progress, fail_commit;
static void *owned[16];
static int pressure_mode, pressure_pops;static CoreQueueMutex *pressure_queue;
static struct {CoreWorkerTaskFn fn;void *context;int index;} pressure_tasks[4];
static void *pressure_task(void *p){
 int index=(int)((__typeof__(pressure_tasks[0])*)p)->index;
 if(index==0){for(int i=0;core_queue_mutex_size(pressure_queue)==0;i++){assert(i<10000);struct timespec d={0,1000000};nanosleep(&d,NULL);}}
 return pressure_tasks[index].fn(pressure_tasks[index].context);
}

static void *fault_calloc(size_t n,size_t size) {
    if(++allocation_index==fail_allocation)return NULL;
    void *p=calloc(n,size);if(p){for(int i=0;i<16;i++)if(!owned[i]){owned[i]=p;outstanding++;break;}}return p;
}
static void fault_free(void *p){if(p)for(int i=0;i<16;i++)if(owned[i]==p){owned[i]=NULL;outstanding--;break;}free(p);}
static bool fault_queue_init(CoreQueueMutex *q,void **slots,size_t capacity){pressure_queue=q;return !fail_queue && core_queue_mutex_init(q,slots,capacity);}
static bool fault_pool_init(CoreWorkers *w,pthread_t *threads,size_t count,CoreWorkerTask *tasks,size_t capacity,CoreQueueMutex *q){return !fail_pool && core_workers_init(w,threads,count,tasks,capacity,q);}
static bool fault_submit(CoreWorkers *w,CoreWorkerTaskFn fn,void *context){if(++submit_index==fail_submit)return false;if(pressure_mode){int index=submit_index-1;assert(index<4);pressure_tasks[index].fn=fn;pressure_tasks[index].context=context;pressure_tasks[index].index=index;return core_workers_submit(w,pressure_task,&pressure_tasks[index]);}return core_workers_submit(w,fn,context);}
static bool fault_pop(CoreQueueMutex *q,void **p,unsigned timeout){if(pressure_mode && !pressure_pops){for(int i=0;core_queue_mutex_size(q)<4;i++){assert(i<10000);struct timespec d={0,1000000};nanosleep(&d,NULL);}assert(q->ring.capacity==4);assert(!core_queue_mutex_push(q,(void*)1));assert(core_queue_mutex_stats(q).drops==0);}
 bool ok=core_queue_mutex_timed_pop(q,p,timeout);if(pressure_mode && ok){if(!pressure_pops)assert(*p!=pressure_tasks[0].context);pressure_pops++;}if(ok && fail_completion){*p=NULL;fail_completion=0;}return ok;}
#define calloc fault_calloc
#define free fault_free
#define core_queue_mutex_init fault_queue_init
#define core_workers_init fault_pool_init
#define core_workers_submit fault_submit
#define core_queue_mutex_timed_pop fault_pop
#include "../src/render/runtime_native_3d_tile_scheduler.c"
#undef calloc
#undef free
#undef core_queue_mutex_init
#undef core_workers_init
#undef core_workers_submit
#undef core_queue_mutex_timed_pop
static atomic_bool final_cancel;
static int cancel_at_commit;
static bool progress(const RuntimeNative3DTileSchedulerProgress *p,void *user){(void)p;(void)user;return !fail_progress;}
static bool commit(const RuntimeNative3DCheckpointTile *tiles,size_t count,int completed,int active,size_t done,size_t total,int frames,void *user){(void)tiles;(void)count;(void)completed;(void)active;(void)done;(void)total;(void)frames;(void)user;if(cancel_at_commit && completed==frames)atomic_store(&final_cancel,true);return !fail_commit;}
int main(void){
 for(int mode=0;mode<17;mode++){
  RuntimeNative3DPreparedFrame frame={0};RuntimeNative3DRenderStats stats={0};uint8_t pixels[32*32*4];
  allocation_index=submit_index=0;pressure_mode=mode==16;pressure_pops=0;fail_allocation=mode>=1&&mode<=7?mode:0;fail_queue=mode==8;fail_pool=mode==9;fail_submit=(mode==10 || mode==15)?2:0;cancel_at_commit=mode==14;atomic_store(&final_cancel,false);fail_completion=mode==11;fail_progress=mode==12;fail_commit=mode==13;
  RuntimeScene3D_Init(&frame.scene);RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
  frame.scene.hasCamera=true;frame.scene.camera.position=vec3(0,-4,1.5);frame.scene.camera.zoom=1;frame.scene.camera.nearPlane=.1;
  assert(RuntimeCameraProjector3D_Build(&frame.scene.camera,32,32,&frame.projector));frame.width=frame.height=32;frame.valid=true;frame.tileOccupancyConservativeAllTiles=true;
  animSettings.tileSize=16;animSettings.renderScale3D=1;animSettings.disneyDenoiseEnabled=false;RuntimeNative3DTileSchedulerResetAdaptivePlan();
  RuntimeNative3DTileSchedulerCheckpointControl checkpoint={.commit=commit,.tileBatchSize=2,.maxTileBatchSize=2};RuntimeNative3DTileSchedulerCancelToken token={.cancelRequested=&final_cancel,.generation=47};RuntimeNative3DTileSchedulerControl control={.checkpoint=&checkpoint,.cancelToken=&token};if(mode==15)checkpoint.tileBatchSize=checkpoint.maxTileBatchSize=1;if(mode==16){checkpoint.tileBatchSize=checkpoint.maxTileBatchSize=4;control.nonInteractive=true;}
  bool ok=RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(pixels,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,&frame,1,NULL,NULL,progress,NULL,NULL,&control,&stats);
  printf("mode=%d ok=%d allocations=%d completed=%d final=%d outstanding=%d\n",mode,ok,allocation_index,stats.temporalCompletedTileJobCount,stats.temporalFinalFullResolveCount,outstanding);fflush(stdout);
  assert(ok==(mode==0 || mode==16));assert(outstanding==0);if(mode && mode!=16)assert(stats.temporalFinalFullResolveCount==0);if(mode==16)assert(pressure_pops==4 && stats.temporalCompletedTileJobCount==4);
  if(mode>=12)assert(stats.temporalCompletedTileJobCount>0);if(mode==14)assert(stats.temporalTileSchedulerFinalResolveBlockedByCancelCount==1);
  RuntimeNative3DPreparedFrame_Free(&frame);
 }
 puts("PASS actual tile allocations, queue/pool/submit/completion and publication refusal");return 0;
}
