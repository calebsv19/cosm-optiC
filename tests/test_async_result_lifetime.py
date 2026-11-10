"""Contained real async-job lifetime and existing regression tests."""
from pathlib import Path
import os, subprocess, tempfile
ROOT = Path(__file__).resolve().parents[1]
PROBE = r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "render/runtime_native_3d_async_render_job.h"
static bool run(const RuntimeNative3DRenderRequestSnapshot *s,const RuntimeNative3DTileSchedulerCancelToken *t,void *u,RuntimeNative3DAsyncRenderJobResult *r){assert(s->cancelTokenBound && t->cancelRequested);r->completionValue=123;return u==NULL;}
int main(void){
 for(int mode=0;mode<4;mode++){
 RuntimeNative3DAsyncRenderJob *j=RuntimeNative3DAsyncRenderJob_Create();assert(j);
 RuntimeNative3DAsyncRenderJobStartDesc d={.snapshot={.valid=true},.generation=1,.run_fn=run,.user_data=mode==2?(void*)1:NULL};
 RuntimeNative3DAsyncRenderJobResult r;
 assert(RuntimeNative3DAsyncRenderJob_Start(j,&d));assert(RuntimeNative3DAsyncRenderJob_Join(j));
 int status=RuntimeNative3DAsyncRenderJob_TryPublish(j,mode==1?2:1,&r);
 assert(status==(mode==1?RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_STALE_GENERATION:mode==2?RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_FAILED:RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED));
 assert(!r.snapshot.cancelTokenBound && !r.snapshot.cancelToken.cancelRequested);
 RuntimeNative3DAsyncRenderJobResult held=r;
 if(mode==3){d.generation=2;assert(RuntimeNative3DAsyncRenderJob_Start(j,&d));assert(RuntimeNative3DAsyncRenderJob_Join(j));}
 RuntimeNative3DAsyncRenderJob_Destroy(j);
 assert(memcmp(&held,&r,sizeof r)==0 && r.completionValue==123 && r.generation==1);
 } puts("PASS held successful, stale and failed results across reuse/destruction");return 0;
}
'''
SUPPORT = r'''#include <assert.h>
#include <stdio.h>
#include "render/runtime_native_3d_tile_scheduler.h"
static int failures;
void assert_true(const char *name,bool ok){if(!ok){fprintf(stderr,"FAIL %s\n",name);failures++;}}
int test_support_failures(void){return failures;}
bool RuntimeNative3DTileSchedulerCancelToken_IsRequested(const RuntimeNative3DTileSchedulerCancelToken *t){return t && t->cancelRequested && atomic_load_explicit(t->cancelRequested,memory_order_acquire);}
int run_test_runtime_native_3d_async_render_job_tests(void);
int main(void){return run_test_runtime_native_3d_async_render_job_tests();}
'''
def main():
 workspace = ROOT.parents[1]
 includes=[ROOT/'include',ROOT/'src',ROOT/'src/tools',ROOT/'tests',ROOT/'third_party/codework_shared/shape/external/cjson',workspace/'fisiCs/include',Path('/opt/homebrew/include'),Path('/opt/homebrew/include/SDL2')]
 includes+=list((ROOT/'third_party/codework_shared').glob('core/*/include'))+list((workspace/'shared/core').glob('*/include'))
 with tempfile.TemporaryDirectory(prefix='optic-l3-result-') as tmp:
  out=Path(tmp);(out/'probe.c').write_text(PROBE);(out/'support.c').write_text(SUPPORT)
  for sanitized in [False,True]:
   flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if sanitized else []
   for regression in [False,True]:
    sources=[out/'support.c',ROOT/'tests/test_runtime_native_3d_async_render_job.c',ROOT/'src/render/runtime_native_3d_render_request_snapshot.c'] if regression else [out/'probe.c']
    sources+=[ROOT/'src/render/runtime_native_3d_async_render_job.c']
    cmd=['/usr/bin/clang','-std=c11','-D_DEFAULT_SOURCE','-g','-Wall','-Wextra','-Werror',*flags,*[f'-I{p}' for p in includes],*map(str,sources),'-o',str(out/'probe')]
    subprocess.run(cmd,check=True)
    subprocess.run([str(out/'probe')],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),check=True)
    print('PASS', 'sanitized' if sanitized else 'strict', 'existing async regression' if regression else 'held result',flush=True)
if __name__=='__main__':main()
