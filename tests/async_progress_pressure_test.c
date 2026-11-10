#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
static int fail_alloc,fail_mutex,fail_resize;
static void *probe_calloc(size_t n,size_t s){return fail_alloc?NULL:calloc(n,s);}
static void *probe_realloc(void *p,size_t n){return fail_resize?NULL:realloc(p,n);}
static int probe_mutex(pthread_mutex_t *m,const pthread_mutexattr_t *a){return fail_mutex?ENOMEM:pthread_mutex_init(m,a);}
#define calloc probe_calloc
#define realloc probe_realloc
#define pthread_mutex_init probe_mutex
#include "../src/render/runtime_native_3d_async_render_bridge.c"
#undef calloc
#undef realloc
#undef pthread_mutex_init
int main(void){
 fail_alloc=1;assert(!RuntimeNative3DAsyncRenderProgressBuffer_Create());fail_alloc=0;
 fail_mutex=1;assert(!RuntimeNative3DAsyncRenderProgressBuffer_Create());fail_mutex=0;
 RuntimeNative3DAsyncRenderProgressBuffer *p=RuntimeNative3DAsyncRenderProgressBuffer_Create();assert(p);
 uint8_t a[16],b[64],held[16],out[64];memset(a,17,sizeof a);memset(b,42,sizeof b);memset(out,99,sizeof out);
 RuntimeNative3DAsyncRenderProgressRect rect={0,0,2,2};
 assert(RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(p,1,a,2,2,rect));
 RuntimeNative3DAsyncRenderProgressSnapshot s;size_t needed=0;
 assert(!RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(p,1,&s,out,1,&needed));assert(needed==16 && out[0]==99);
 fail_resize=1;assert(!RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(p,2,b,4,4,(RuntimeNative3DAsyncRenderProgressRect){0,0,4,4}));fail_resize=0;
 assert(RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(p,1,&s,held,sizeof held,&needed));assert(!memcmp(a,held,16));
 for(int i=0;i<1000;i++)assert(RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(p,2,b,4,4,(RuntimeNative3DAsyncRenderProgressRect){i%4,(i/4)%4,1,1}));
 assert(!RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(p,1,&s,out,sizeof out,&needed));assert(s.staleGeneration);
 assert(RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(p,2,&s,out,sizeof out,&needed));assert(needed==64 && !memcmp(out,b,64));
 assert(!RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(p,2,&s,out,sizeof out,&needed));
 IntegratorTile tile={0};RuntimeNative3DTileSchedulerProgress t={.dirtyTiles=&tile,.dirtyTileCount=1,.completedTilesInSubpass=7};
 assert(RuntimeNative3DAsyncRenderProgressBuffer_PublishTileProgress(p,2,&t));t.completedTilesInSubpass=99;
 RuntimeNative3DAsyncRenderTileProgressSnapshot copied;
 assert(RuntimeNative3DAsyncRenderProgressBuffer_CopyLatestTileProgress(p,2,&copied));assert(copied.completedTilesInSubpass==7);
 RuntimeNative3DAsyncRenderProgressBuffer_Reset(p);RuntimeNative3DAsyncRenderProgressBuffer_Destroy(p);
 assert(!memcmp(a,held,16) && copied.completedTilesInSubpass==7);
 puts("PASS progress pressure, allocation refusal, short reader, stale generation and held copies");return 0;
}
