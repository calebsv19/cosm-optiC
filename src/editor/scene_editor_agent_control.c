#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "editor/scene_editor_agent_control.h"
#include "editor/scene_editor_document.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

enum {CLIENTS=4,CAPACITY=65536};
static struct {
    int listener;
    char socket_path[108],descriptor[PATH_MAX],scene[PATH_MAX];
    struct {int fd;size_t used,sent,total;double activity;char input[CAPACITY],output[CAPACITY];} clients[CLIENTS];
} control={.listener=-1};
static bool registered;
/* Keep every published scene binding for the lifetime of this endpoint.
   Matrix runs can visit many scenes; evicting old bindings disconnects clients. */
typedef struct SceneAttachment {
    struct SceneAttachment* next;
    char descriptor[];
} SceneAttachment;
static SceneAttachment* aliases;
static bool reserved;
static double now(void) {
    struct timespec stamp;clock_gettime(CLOCK_MONOTONIC,&stamp);
    return stamp.tv_sec+stamp.tv_nsec/1e9;
}
static bool reclaim_stale_descriptor(const char* path,const char* scene) {
    struct stat st;
    if(lstat(path,&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=getuid()) return false;
    json_object* info=json_object_from_file(path),*schema=NULL,*owner=NULL,*pid=NULL,*socket_path=NULL;
    bool valid=info && json_object_object_get_ex(info,"schema_version",&schema) &&
        json_object_is_type(schema,json_type_string) && !strcmp(json_object_get_string(schema),"optic_agent_control_v1") &&
        json_object_object_get_ex(info,"scene_path",&owner) && json_object_is_type(owner,json_type_string) &&
        !strcmp(json_object_get_string(owner),scene) && json_object_object_get_ex(info,"pid",&pid) &&
        json_object_is_type(pid,json_type_int) && json_object_get_int64(pid)>0 && json_object_get_int64(pid)<=INT_MAX &&
        json_object_object_get_ex(info,"socket",&socket_path) && json_object_is_type(socket_path,json_type_string);
    if(valid) {
        char expected[108];pid_t previous=(pid_t)json_object_get_int64(pid);
        snprintf(expected,sizeof(expected),"/tmp/optic-agent-%u-%ld.sock",(unsigned)getuid(),(long)previous);
        valid=!strcmp(expected,json_object_get_string(socket_path)) && kill(previous,0)<0 && errno==ESRCH;
    }
    if(info) json_object_put(info);
    return valid && unlink(path)==0;
}
void SceneEditorAgentControlClose(void) {
    if(control.listener<0) return;
    close(control.listener);
    for(int i=0;i<CLIENTS;++i) if(control.clients[i].fd>=0) close(control.clients[i].fd);
    unlink(control.socket_path);
    json_object* descriptor=json_object_from_file(control.descriptor),*path=NULL;
    if(descriptor && json_object_object_get_ex(descriptor,"socket",&path) &&
       json_object_is_type(path,json_type_string) && !strcmp(json_object_get_string(path),control.socket_path))
        unlink(control.descriptor);
    if(descriptor) json_object_put(descriptor);
    while(aliases) {
        SceneAttachment* attachment=aliases;aliases=attachment->next;
        json_object* info=json_object_from_file(attachment->descriptor),*pid=NULL;
        if(info && json_object_object_get_ex(info,"pid",&pid) && json_object_get_int64(pid)==getpid()) unlink(attachment->descriptor);
        if(info) json_object_put(info);
        free(attachment);
    }
    reserved=false;
    memset(&control,0,sizeof(control));control.listener=-1;
}
static bool begin(void) {
    const char* scene=SceneEditorDocumentPath();
    if(!scene || strlen(scene)>=sizeof(control.scene)) return false;
    snprintf(control.scene,sizeof(control.scene),"%s",scene);
    snprintf(control.socket_path,sizeof(control.socket_path),"/tmp/optic-agent-%u-%ld.sock",(unsigned)getuid(),(long)getpid());
    if(snprintf(control.descriptor,sizeof(control.descriptor),"%s.optic-agent.json",scene)>=(int)sizeof(control.descriptor)) return false;
    /* Preserve a live writer. Reclaim only our typed descriptor whose recorded
       process is authoritatively absent; unknown ownership remains untouched. */
    int descriptor=open(control.descriptor,O_CREAT|O_EXCL|O_WRONLY,0600);
    if(descriptor<0 && errno==EEXIST && reclaim_stale_descriptor(control.descriptor,scene))
        descriptor=open(control.descriptor,O_CREAT|O_EXCL|O_WRONLY,0600);
    if(descriptor<0) return false;
    control.listener=socket(AF_UNIX,SOCK_STREAM,0);
    struct sockaddr_un address={0};address.sun_family=AF_UNIX;
    snprintf(address.sun_path,sizeof(address.sun_path),"%s",control.socket_path);
    bool ok=control.listener>=0 && bind(control.listener,(struct sockaddr*)&address,sizeof(address))==0 &&
        chmod(control.socket_path,0600)==0 && listen(control.listener,CLIENTS)==0 &&
        fcntl(control.listener,F_SETFL,O_NONBLOCK)==0 && fcntl(control.listener,F_SETFD,FD_CLOEXEC)==0;
    if(!ok) {
        if(control.listener>=0) close(control.listener);control.listener=-1;
        close(descriptor);unlink(control.descriptor);unlink(control.socket_path);return false;
    }
    for(int i=0;i<CLIENTS;++i) control.clients[i].fd=-1;
    json_object* info=json_object_new_object();
    json_object_object_add(info,"schema_version",json_object_new_string("optic_agent_control_v1"));
    json_object_object_add(info,"socket",json_object_new_string(control.socket_path));
    json_object_object_add(info,"scene_path",json_object_new_string(scene));
    json_object_object_add(info,"pid",json_object_new_int64(getpid()));
    const char* data=json_object_to_json_string_ext(info,JSON_C_TO_STRING_PLAIN);
    size_t len=strlen(data);ssize_t written=write(descriptor,data,len);close(descriptor);json_object_put(info);
    if(written!=(ssize_t)len) {SceneEditorAgentControlClose();return false;}
    if(!registered) {atexit(SceneEditorAgentControlClose);registered=true;}
    return true;
}
bool SceneEditorAgentControlReservePath(const char* scene,char* message,size_t size) {
    char descriptor[PATH_MAX];
    reserved=false;
    if(control.listener<0 || snprintf(descriptor,sizeof(descriptor),"%s.optic-agent.json",scene)>=(int)sizeof(descriptor)) {
        snprintf(message,size,"No active editor endpoint or path too long.");return false;
    }
    if(!strcmp(descriptor,control.descriptor)) return true;
    for(SceneAttachment* attachment=aliases;attachment;attachment=attachment->next)
        if(!strcmp(descriptor,attachment->descriptor)) return true;
    /* Allocate before publishing: failure leaves the scene and endpoint intact. */
    SceneAttachment* attachment=malloc(sizeof(*attachment)+strlen(descriptor)+1);
    if(!attachment) {snprintf(message,size,"Cannot allocate editor attachment.");return false;}
    strcpy(attachment->descriptor,descriptor);
    int fd=open(descriptor,O_CREAT|O_EXCL|O_WRONLY,0600);
    if(fd<0 && errno==EEXIST && reclaim_stale_descriptor(descriptor,scene)) fd=open(descriptor,O_CREAT|O_EXCL|O_WRONLY,0600);
    if(fd<0) {free(attachment);snprintf(message,size,"Destination has an existing editor endpoint or cannot be attached.");return false;}
    json_object* info=json_object_new_object();
    json_object_object_add(info,"schema_version",json_object_new_string("optic_agent_control_v1"));
    json_object_object_add(info,"socket",json_object_new_string(control.socket_path));
    json_object_object_add(info,"scene_path",json_object_new_string(scene));
    json_object_object_add(info,"pid",json_object_new_int64(getpid()));
    const char* text=json_object_to_json_string_ext(info,JSON_C_TO_STRING_PLAIN);size_t len=strlen(text);
    bool ok=write(fd,text,len)==(ssize_t)len;close(fd);json_object_put(info);
    if(!ok) {unlink(descriptor);free(attachment);snprintf(message,size,"Cannot publish editor attachment.");return false;}
    attachment->next=aliases;aliases=attachment;reserved=true;return true;
}
void SceneEditorAgentControlReleaseReservedPath(void) {
    if(reserved && aliases) {
        SceneAttachment* attachment=aliases;aliases=attachment->next;
        unlink(attachment->descriptor);free(attachment);
    }
    reserved=false;
}

bool SceneEditorAgentControlPoll(void) {
    if(!SceneEditorDocumentIsOpen()) {SceneEditorAgentControlClose();return false;}
    /* Lifecycle commands reserve an alias before switching; keep live replies. */
    if(control.listener<0 && !begin()) return false;
    bool changed=false;
    for(int i=0;i<CLIENTS;++i) {
        if(control.clients[i].fd>=0 && now()-control.clients[i].activity>5) {
            close(control.clients[i].fd);control.clients[i].fd=-1;
        }
        if(control.clients[i].fd<0) {
            int fd=accept(control.listener,NULL,NULL);
            if(fd<0) continue;
            fcntl(fd,F_SETFL,O_NONBLOCK);
            fcntl(fd,F_SETFD,FD_CLOEXEC);
#ifdef __APPLE__
            int enabled=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&enabled,sizeof(enabled));
#endif
            control.clients[i].fd=fd;control.clients[i].used=control.clients[i].total=control.clients[i].sent=0;
            control.clients[i].activity=now();
        }
        int fd=control.clients[i].fd;
        if(!control.clients[i].total) {
            size_t used=control.clients[i].used;
            ssize_t bytes=read(fd,control.clients[i].input+used,CAPACITY-1-used);
            if(bytes<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) continue;
            if(bytes<=0) {close(fd);control.clients[i].fd=-1;continue;}
            control.clients[i].activity=now();
            used+=(size_t)bytes;control.clients[i].used=used;control.clients[i].input[used]=0;
            char* newline=memchr(control.clients[i].input,'\n',used);
            if(!newline && used<CAPACITY-1) continue;
            if(newline) *newline=0;
            json_object* request=newline?json_tokener_parse(control.clients[i].input):NULL;
            json_object* response=SceneEditorAgentControlDispatch(request);
            json_object* success=NULL;
            if(json_object_object_get_ex(response,"ok",&success) && json_object_get_boolean(success)) changed=true;
            const char* encoded=json_object_to_json_string_ext(response,JSON_C_TO_STRING_PLAIN);
            int count=snprintf(control.clients[i].output,CAPACITY,"%s\n",encoded);
            if(count<0 || count>=CAPACITY) {close(fd);control.clients[i].fd=-1;}
            else control.clients[i].total=(size_t)count;
            if(request) json_object_put(request);json_object_put(response);
        }
        if(control.clients[i].fd>=0 && control.clients[i].total) {
            ssize_t bytes=send(fd,control.clients[i].output+control.clients[i].sent,
                control.clients[i].total-control.clients[i].sent,
#ifdef MSG_NOSIGNAL
                MSG_NOSIGNAL
#else
                0
#endif
            );
            if(bytes>0) {control.clients[i].sent+=(size_t)bytes;control.clients[i].activity=now();}
            if(control.clients[i].sent==control.clients[i].total ||
               (bytes<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR)) {
                close(fd);control.clients[i].fd=-1;
            }
        }
    }
    return changed;
}
