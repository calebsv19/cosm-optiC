#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
static int read_interrupt, wait_interrupt, reap_interrupt;
static int setup_calls, setup_fail, fork_fail;
static int failed_fcntl(int fd,int command,...){va_list ap;va_start(ap,command);int arg=va_arg(ap,int);va_end(ap);if(++setup_calls==setup_fail){errno=EMFILE;return -1;}return fcntl(fd,command,arg);}
static pid_t failed_fork(void){if(fork_fail){errno=EAGAIN;return -1;}return fork();}
#define fcntl failed_fcntl
#define fork failed_fork
static ssize_t interrupted_read(int fd,void *buf,size_t n){if(read_interrupt){read_interrupt=0;errno=EINTR;return -1;}return read(fd,buf,n);}
static int interrupted_waitid(idtype_t type,id_t id,siginfo_t *info,int options){if(wait_interrupt){wait_interrupt=0;errno=EINTR;return -1;}return waitid(type,id,info,options);}
static pid_t interrupted_waitpid(pid_t pid,int *status,int options){if(reap_interrupt){reap_interrupt=0;errno=EINTR;return -1;}return waitpid(pid,status,options);}
#define read interrupted_read
#define waitid interrupted_waitid
#define waitpid interrupted_waitpid
#include "../src/platform/ray_tracing_folder_picker.c"
#undef fcntl
#undef fork
#undef read
#undef waitid
#undef waitpid
static int open_fds(void){int n=0;for(int i=0;i<256;i++)if(fcntl(i,F_GETFD)>=0)n++;return n;}
int main(void){
 const char *refused_args[]={"/usr/bin/printf","/tmp\n",NULL};
 for(int fail=1;fail<=5;fail++){
  RayTracingFolderPickerRequest refused;RayTracing_FolderPicker_RequestInit(&refused);
  int before=open_fds();setup_calls=0;setup_fail=fail<5?fail:0;fork_fail=fail==5;
  assert(!start_picker_process(&refused,refused_args));
  assert(!refused.active && refused.child_pid==0 && refused.output_fd==-1);
  assert(open_fds()==before);
 }
 setup_fail=fork_fail=0;puts("PASS picker descriptor and fork admission refusal without leaks");
 RayTracingFolderPickerRequest launched;RayTracing_FolderPicker_RequestInit(&launched);
 const char *args[]={"/usr/bin/printf","/tmp\n",NULL};
 assert(start_picker_process(&launched,args));pid_t launched_pid=launched.child_pid;
 char selected[256];read_interrupt=wait_interrupt=reap_interrupt=1;
 RayTracingFolderPickerResult result=RAY_TRACING_FOLDER_PICKER_PENDING;
 for(int i=0;i<100 && result==RAY_TRACING_FOLDER_PICKER_PENDING;i++){result=RayTracing_FolderPicker_Poll(&launched,selected,sizeof(selected));struct timespec delay={0,10000000};nanosleep(&delay,NULL);}
 assert(result==RAY_TRACING_FOLDER_PICKER_SELECTED);assert(!strcmp(selected,"/tmp"));
 assert(!read_interrupt && !wait_interrupt && !reap_interrupt);
 assert(waitpid(launched_pid,NULL,WNOHANG)==-1 && errno==ECHILD);
 puts("PASS actual picker launch and interrupted read/wait/reap");

 int pipes[2];assert(pipe(pipes)==0);pid_t leader=fork();assert(leader>=0);
 if(!leader){close(pipes[0]);assert(setpgid(0,0)==0);pid_t helper=fork();assert(helper>=0);if(helper)_exit(0);signal(SIGTERM,SIG_IGN);assert(write(pipes[1],"x",1)==1);for(;;)pause();}
 close(pipes[1]);char byte;assert(read(pipes[0],&byte,1)==1);
 RayTracingFolderPickerRequest request;RayTracing_FolderPicker_RequestInit(&request);request.active=true;request.child_pid=leader;
 RayTracing_FolderPicker_Cancel(&request);
 struct pollfd p={.fd=pipes[0],.events=POLLIN|POLLHUP};int ready=poll(&p,1,1000);
 if(ready<=0)kill(-leader,SIGKILL);
 assert(ready>0 && read(pipes[0],&byte,1)==0);close(pipes[0]);assert(waitpid(leader,NULL,WNOHANG)==-1 && errno==ECHILD);
 puts("PASS picker exited leader descendant cleanup");return 0;
}
