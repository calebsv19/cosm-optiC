#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <spawn.h>
#include <errno.h>
static int fail_spawn;
static int probe_actions(posix_spawn_file_actions_t *a){return fail_spawn==1?ENOMEM:posix_spawn_file_actions_init(a);}
static int probe_attr(posix_spawnattr_t *a){return fail_spawn==2?ENOMEM:posix_spawnattr_init(a);}
static int probe_dup(posix_spawn_file_actions_t *a,int f,int t){return fail_spawn==3?ENOMEM:posix_spawn_file_actions_adddup2(a,f,t);}
static int probe_spawn(pid_t *p,const char *f,const posix_spawn_file_actions_t *a,const posix_spawnattr_t *t,char *const v[],char *const e[]){return fail_spawn==4?EAGAIN:posix_spawnp(p,f,a,t,v,e);}
#define posix_spawn_file_actions_init probe_actions
#define posix_spawnattr_init probe_attr
#define posix_spawn_file_actions_adddup2 probe_dup
#define posix_spawnp probe_spawn
#define RAY_RIGID_RESULT_ENABLED
#include "../src/editor/scene_editor_rigid_job.c"
bool SceneEditorDocumentIsOpen(void){return true;}
bool SceneEditorDocumentIsDirty(void){return false;}
const char *SceneEditorDocumentPath(void){return "/test-source";}
unsigned long long SceneEditorDocumentRevision(void){return 1;}
bool SceneEditorDocumentSwitch(const char *p,bool h,char *m,size_t n){(void)p;(void)h;(void)m;(void)n;return false;}
void SceneEditorTimelinePause(void){}
int main(void){
 int fds[2];assert(pipe(fds)==0);pid_t leader=fork();assert(leader>=0);
 if(!leader){close(fds[0]);assert(setpgid(0,0)==0);pid_t helper=fork();assert(helper>=0);if(helper)_exit(0);signal(SIGTERM,SIG_IGN);assert(write(fds[1],"x",1)==1);for(;;)pause();}
 close(fds[1]);char c;assert(read(fds[0],&c,1)==1);child=leader;char message[512];
 for(int i=0;i<100 && child;i++){SceneEditorRigidJobPoll(message,sizeof message);struct timespec t={0,1000000};nanosleep(&t,NULL);}
 assert(!child);struct pollfd p={.fd=fds[0],.events=POLLIN|POLLHUP};int ready=poll(&p,1,1000);
 if(ready<=0)kill(-leader,SIGKILL); /* exact fixture group cleanup before assertion */
 assert(ready>0 && read(fds[0],&c,1)==0);close(fds[0]);
 assert(waitpid(leader,NULL,WNOHANG)==-1 && errno==ECHILD);
 char workflow[]="/tmp/optic-rigid-l3-script-XXXXXX",profile[]="/tmp/optic-rigid-l3-profile-XXXXXX";
 int script=mkstemp(workflow),pf=mkstemp(profile);assert(script>=0 && pf>=0);
 const char *code="import signal,time\nprint('ready',flush=True)\nsignal.signal(signal.SIGTERM,signal.SIG_IGN)\ntime.sleep(60)\n";
 assert(write(script,code,strlen(code))==(ssize_t)strlen(code));close(script);
 FILE *f=fdopen(pf,"w");assert(f);fprintf(f,"{\"schema\":\"optic_rigid_run_profile_v1\",\"workflow\":\"%s\",\"ball_tool\":\"/unused\",\"backend\":\"/unused\",\"pack_io\":\"/unused\",\"assets\":\"/unused\",\"store\":\"/unused\",\"renderer\":\"/unused\",\"output\":\"/unused\",\"ticks\":\"1\",\"expected_head\":\"none\"}",workflow);fclose(f);
 for(fail_spawn=1;fail_spawn<=4;fail_spawn++){assert(!SceneEditorRigidJobBegin(profile,message,sizeof message));assert(!SceneEditorRigidJobBusy());assert(access(log_path,F_OK)!=0);}
 fail_spawn=0;
 assert(SceneEditorRigidJobBegin(profile,message,sizeof message));pid_t admitted=child;assert(getpgid(child)==child);
 assert(!SceneEditorRigidJobBegin(profile,message,sizeof message));
 for(int i=0;i<100;i++){f=fopen(log_path,"r");int c=f?fgetc(f):EOF;if(f)fclose(f);if(c!=EOF)break;struct timespec t={0,10000000};nanosleep(&t,NULL);}
 SceneEditorRigidJobShutdown();assert(!SceneEditorRigidJobBusy());assert(waitpid(admitted,NULL,WNOHANG)==-1 && errno==ECHILD);
 SceneEditorRigidJobShutdown();unlink(workflow);unlink(profile);unlink(log_path);
 puts("PASS rigid spawn group, busy refusal, pending shutdown, exited leader descendants and direct reap");return 0;
}
