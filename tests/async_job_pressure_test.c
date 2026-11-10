#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
static int fail_alloc,fail_mutex,fail_thread;
static void *probe_calloc(size_t n,size_t s){return fail_alloc?NULL:calloc(n,s);}
static int probe_mutex(pthread_mutex_t *m,const pthread_mutexattr_t *a){return fail_mutex?ENOMEM:pthread_mutex_init(m,a);}
static int probe_thread(pthread_t *t,const pthread_attr_t *a,void *(*f)(void*),void *p){return fail_thread?EAGAIN:pthread_create(t,a,f,p);}
#define calloc probe_calloc
#define pthread_mutex_init probe_mutex
#define pthread_create probe_thread
#include "../src/render/runtime_native_3d_async_render_job.c"
#undef calloc
#undef pthread_mutex_init
#undef pthread_create
static bool run(const RuntimeNative3DRenderRequestSnapshot *s,const RuntimeNative3DTileSchedulerCancelToken *t,void *u,RuntimeNative3DAsyncRenderJobResult *r){(void)s;(void)t;(void)u;r->completionValue=29;return true;}
int main(void){
 fail_alloc=1;assert(!RuntimeNative3DAsyncRenderJob_Create());fail_alloc=0;
 fail_mutex=1;assert(!RuntimeNative3DAsyncRenderJob_Create());fail_mutex=0;
 RuntimeNative3DAsyncRenderJob *j=RuntimeNative3DAsyncRenderJob_Create();assert(j);
 RuntimeNative3DAsyncRenderJobStartDesc d={.snapshot={.valid=true},.generation=1,.run_fn=run};
 fail_thread=1;assert(!RuntimeNative3DAsyncRenderJob_Start(j,&d));
 assert(RuntimeNative3DAsyncRenderJob_GetStatus(j)==RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE);
 fail_thread=0;assert(RuntimeNative3DAsyncRenderJob_Start(j,&d));assert(!RuntimeNative3DAsyncRenderJob_Start(j,&d));assert(RuntimeNative3DAsyncRenderJob_Join(j));
 fail_thread=1;d.generation=2;assert(!RuntimeNative3DAsyncRenderJob_Start(j,&d));
 RuntimeNative3DAsyncRenderJobResult held;
 assert(RuntimeNative3DAsyncRenderJob_TryPublish(j,1,&held)==RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED);
 assert(held.generation==1 && held.completionValue==29 && held.succeeded);
 fail_thread=0;assert(RuntimeNative3DAsyncRenderJob_Start(j,&d));assert(RuntimeNative3DAsyncRenderJob_ShutdownCancelFirst(j));
 RuntimeNative3DAsyncRenderJob_Destroy(j);assert(held.generation==1 && !held.snapshot.cancelToken.cancelRequested);
 puts("PASS allocation/mutex/thread refusal, retained completion, capacity and cancel/join");return 0;
}
