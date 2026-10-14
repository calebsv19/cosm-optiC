#include "ui/scene_catalog_labels.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *paths[]={"/workspace/ray_tracing/config/samples/demo_runtime.json",
        "/workspace/_worktrees/ray_tracing_main_edit/config/samples/demo_runtime.json",
        "/workspace/ray_tracing/config/samples/another_runtime.json",
        "/libraries/room/scene_runtime.json","/libraries/other/room/scene_runtime.json"};
    char a[4096],b[4096];
    scene_catalog_directory_label(paths[0],paths,5,a,sizeof(a));
    scene_catalog_directory_label(paths[1],paths,5,b,sizeof(b));
    assert(!strcmp(a,"ray_tracing/config/samples"));
    assert(!strcmp(b,"ray_tracing_main_edit/config/samples"));
    scene_catalog_directory_label(paths[2],paths,5,b,sizeof(b));assert(!strcmp(a,b));
    scene_catalog_directory_label(paths[3],paths,5,a,sizeof(a));
    scene_catalog_directory_label(paths[4],paths,5,b,sizeof(b));assert(strcmp(a,b));
    assert(!strcmp(a,"libraries/room") && !strcmp(b,"other/room"));
    scene_catalog_directory_label(NULL,paths,5,a,sizeof(a));assert(!*a);
    scene_catalog_directory_label("relative_runtime.json",paths,5,a,sizeof(a));assert(!strcmp(a,"Current directory"));
    char tiny[4];scene_catalog_directory_label(paths[0],paths,5,tiny,sizeof(tiny));assert(tiny[3]=='\0');
    puts("scene catalog provenance: distinct roots, same-directory copies, nested scenes and bounded output PASS");
    return 0;
}
