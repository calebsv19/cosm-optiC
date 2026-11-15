/* Writer failure/cancellation/publication contract, independent of rendering. */
#include "render/runtime_spectral_surface_path_internal.h"
#include "app/ray_tracing_sha256.h"
#include <assert.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void no_temporary(const char *root) {
    DIR *d=opendir(root);assert(d);struct dirent *e;
    while((e=readdir(d)))assert(!strstr(e->d_name,".tmp."));
    closedir(d);
}
int main(void) {
    char root[]="/tmp/optic-components-XXXXXX";assert(mkdtemp(root));
    char path[4096];snprintf(path,sizeof(path),"%s/frame.bmp.components.npy",root);
    RuntimeSpectralPathProbe p={.total={1,2,3},.direct={1,2,3}};
    RuntimeSpectralSurfacePathReadout out={0};
    assert(!spectral_components_begin("/nonexistent-optic-dir/frame.npy",1,1));
    assert(!spectral_components_begin(path,0,1));
    SpectralComponentWriter *w=spectral_components_begin(path,2,1);assert(w);
    assert(spectral_components_pixel(w,&p));
    spectral_components_abort(w);assert(access(path,F_OK)!=0);no_temporary(root);
    w=spectral_components_begin(path,2,1);assert(w);
    assert(spectral_components_pixel(w,&p));
    assert(!spectral_components_finish(w,path,false,&out));assert(!out.components_written);
    assert(access(path,F_OK)!=0);no_temporary(root);
    w=spectral_components_begin(path,1,1);assert(w);
    assert(spectral_components_pixel(w,&p));assert(!spectral_components_pixel(w,&p));
    /* Simulate another publisher winning the no-overwrite race. */
    FILE *sentinel=fopen(path,"wb");assert(sentinel);fputs("keep",sentinel);fclose(sentinel);
    assert(!spectral_components_finish(w,path,false,&out));assert(!out.components_written);
    sentinel=fopen(path,"rb");char old[5]={0};assert(fread(old,1,4,sentinel)==4);fclose(sentinel);assert(!strcmp(old,"keep"));no_temporary(root);
    w=spectral_components_begin(path,1,1);assert(w);assert(spectral_components_pixel(w,&p));
    assert(spectral_components_finish(w,path,true,&out));assert(out.components_written && out.components_bytes==392);
    char hash[65];assert(ray_tracing_sha256_file(path,hash));assert(!strcmp(hash,out.components_sha256));
    no_temporary(root);unlink(path);assert(rmdir(root)==0);
    puts("PASS component cancellation, short film, overflow, no-clobber race, overwrite and hash");
}
