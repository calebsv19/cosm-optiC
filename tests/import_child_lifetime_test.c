#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <time.h>
#ifndef L3_PANEL_SOURCE
#define L3_PANEL_SOURCE "../src/editor/scene_editor_transform_panel.c"
#endif
#include L3_PANEL_SOURCE
void SceneEditorSurfaceMappingPanelReset(void){}
void SceneEditorManagedImportCanceled(void){}
void scene_editor_shared_field_end(char *owner){(void)owner;}
void RayTracing_FolderPicker_Cancel(RayTracingFolderPickerRequest *r){(void)r;assert(0);}
int main(void){
 int fds[2];assert(pipe(fds)==0);pid_t p=fork();assert(p>=0);
 if(!p){close(fds[0]);assert(setpgid(0,0)==0);signal(SIGTERM,SIG_IGN);assert(write(fds[1],"x",1)==1);for(;;)pause();}
 close(fds[1]);char byte;assert(read(fds[0],&byte,1)==1);close(fds[0]);printf("owned_child=%ld\n",(long)p);fflush(stdout);
 s_job_pid=p;s_job_kind=TRANSFORM_PANEL_JOB_IMPORT;
 struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);SceneEditorTransformPanelReset();clock_gettime(CLOCK_MONOTONIC,&b);
 assert((b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)/1e9<1.0);assert(s_job_pid==-1);assert(waitpid(p,NULL,WNOHANG)==-1 && errno==ECHILD);
 puts("PASS actual import Reset terminates and reaps TERM-resistant helper");return 0;
}
