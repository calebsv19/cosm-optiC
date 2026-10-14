#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <assert.h>
static int fail_allocation;
static void* attachment_malloc(size_t bytes) {return fail_allocation?NULL:malloc(bytes);}
#define malloc attachment_malloc
#include "../src/editor/scene_editor_agent_control.c"
#undef malloc
static char initial[PATH_MAX];
const char* SceneEditorDocumentPath(void) {return initial;}
bool SceneEditorDocumentIsOpen(void) {return true;}
json_object* SceneEditorAgentControlDispatch(json_object* request) {(void)request;return json_object_new_object();}
int main(int argc,char** argv) {
    assert(argc==2);char message[256],path[PATH_MAX],descriptor[PATH_MAX];
    snprintf(initial,sizeof(initial),"%s/original.json",argv[1]);assert(begin());
    for(int i=0;i<96;++i) {
        snprintf(path,sizeof(path),"%s/scene-%03d.json",argv[1],i);
        assert(SceneEditorAgentControlReservePath(path,message,sizeof(message)));
    }
    SceneAttachment* head=aliases;
    assert(SceneEditorAgentControlReservePath(path,message,sizeof(message)) && aliases==head);
    SceneEditorAgentControlReleaseReservedPath();assert(aliases==head); // Duplicate is never removed.
    fail_allocation=1;snprintf(path,sizeof(path),"%s/no-memory.json",argv[1]);
    assert(!SceneEditorAgentControlReservePath(path,message,sizeof(message)) && aliases==head);
    snprintf(descriptor,sizeof(descriptor),"%s.optic-agent.json",path);assert(access(descriptor,F_OK)<0);fail_allocation=0;
    snprintf(path,sizeof(path),"%s/rollback.json",argv[1]);assert(SceneEditorAgentControlReservePath(path,message,sizeof(message)));
    snprintf(descriptor,sizeof(descriptor),"%s.optic-agent.json",path);assert(access(descriptor,F_OK)==0);
    SceneEditorAgentControlReleaseReservedPath();assert(aliases==head && access(descriptor,F_OK)<0);
    snprintf(path,sizeof(path),"%s/foreign.json",argv[1]);snprintf(descriptor,sizeof(descriptor),"%s.optic-agent.json",path);
    FILE* file=fopen(descriptor,"w");assert(file);fputs("{\"foreign\":true}",file);fclose(file);
    assert(!SceneEditorAgentControlReservePath(path,message,sizeof(message)) && aliases==head);
    SceneEditorAgentControlClose();assert(!aliases && control.listener==-1 && access(descriptor,F_OK)==0);
    for(int i=0;i<96;++i) {snprintf(descriptor,sizeof(descriptor),"%s/scene-%03d.json.optic-agent.json",argv[1],i);assert(access(descriptor,F_OK)<0);}
    assert(begin());assert(SceneEditorAgentControlReservePath(path,message,sizeof(message))==false);SceneEditorAgentControlClose();snprintf(descriptor,sizeof(descriptor),"%s/foreign.json.optic-agent.json",argv[1]);unlink(descriptor);
    puts("Scene attachments: 96 bindings, duplicate, rollback, allocation failure, ownership and close/reopen PASS");return 0;
}
