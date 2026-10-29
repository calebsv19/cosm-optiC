#define _DEFAULT_SOURCE
#include "editor/scene_editor_document.h"
#include "../src/editor/scene_editor_rigid_job.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
int main(int argc,char **argv) {
 assert(argc==6);char message[2048]={0};
 assert(SceneEditorDocumentSwitch(argv[1],false,message,sizeof(message)));
 unsigned long long revision=SceneEditorDocumentRevision();
 for(int i=2;i<5;i++) {
  assert(!SceneEditorRigidJobBegin(argv[i],message,sizeof(message)));
  assert(!SceneEditorRigidJobBusy() && SceneEditorDocumentRevision()==revision);
 }
 assert(SceneEditorRigidJobBegin(argv[5],message,sizeof(message)));
 for(int i=0;SceneEditorRigidJobBusy()&&i<1000;i++){SceneEditorRigidJobPoll(message,sizeof(message));usleep(10000);}
 assert(!SceneEditorRigidJobBusy() && SceneEditorDocumentRevision()==revision && !SceneEditorDocumentIsDirty());
 SceneEditorDocumentClose();puts("minute job profile accepted; invalid tick/budget/type refused; failed child preserves document");return 0;
}
