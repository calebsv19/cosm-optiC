/* Ordinary V3 linear film output. Streaming NPY v1, with same-directory atomic
 * publication after successful full-film completion. No transport RNG or display
 * conversion is performed here. */
#include "runtime_spectral_surface_path_internal.h"
#include "app/ray_tracing_sha256.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define COMPONENT_COUNT 11
struct SpectralComponentWriter {
    FILE *file;
    char temporary[4096];
    size_t pixels,expected,bytes;
    RayTracingSha256Context hash;
};

bool RuntimeSpectralSurfacePath_ConfigureOutput(RuntimeSpectralSurfacePath *state,
                                               const char *image,bool overwrite) {
    if(!state || !image || !*image || state->component_writer)return false;
    int n=snprintf(state->component_path,sizeof(state->component_path),"%s.components.npy",image);
    if(n<0 || (size_t)n>=sizeof(state->component_path)-16) {state->component_path[0]=0;return false;}
    const char *name=strrchr(state->component_path,'/');name=name?name+1:state->component_path;
    if(strlen(name)>=sizeof(((RuntimeSpectralSurfacePathReadout *)0)->components_file))return false;
    struct stat st;
    if(!overwrite && (lstat(state->component_path,&st)==0 || errno!=ENOENT))return false;
    state->component_overwrite=overwrite;return true;
}
static bool bytes(SpectralComponentWriter *w,const void *data,size_t size) {
    if(fwrite(data,1,size,w->file)!=size)return false;
    ray_tracing_sha256_update(&w->hash,data,size);w->bytes+=size;return true;
}
void spectral_components_abort(SpectralComponentWriter *w) {
    if(!w)return;
    if(w->file)fclose(w->file);
    if(w->temporary[0])unlink(w->temporary);
    free(w);
}
SpectralComponentWriter *spectral_components_begin(const char *path,int width,int height) {
    if(width<1 || height<1 || sizeof(double)!=8)return NULL;
    SpectralComponentWriter *w=calloc(1,sizeof(*w));if(!w)return NULL;
    w->expected=(size_t)width*height;
    int n=snprintf(w->temporary,sizeof(w->temporary),"%s.tmp.XXXXXX",path);
    if(n<0 || (size_t)n>=sizeof(w->temporary)) {free(w);return NULL;}
    int fd=mkstemp(w->temporary);
    if(fd<0) {free(w);return NULL;}
    w->file=fdopen(fd,"wb");
    if(!w->file) {close(fd);spectral_components_abort(w);return NULL;}
    ray_tracing_sha256_init(&w->hash);
    char header[256];
    n=snprintf(header,sizeof(header),"{'descr': '<f8', 'fortran_order': False, 'shape': (%d, %d, %d, 3), }",height,width,COMPONENT_COUNT);
    size_t length=(size_t)n+1;
    while((10+length)%64)++length;
    memset(header+n,' ',length-(size_t)n);header[length-1]='\n';
    unsigned char prefix[]={0x93,'N','U','M','P','Y',1,0,(unsigned char)length,(unsigned char)(length>>8)};
    if(!bytes(w,prefix,sizeof(prefix)) || !bytes(w,header,length)) {spectral_components_abort(w);return NULL;}
    return w;
}
bool spectral_components_pixel(SpectralComponentWriter *w,const RuntimeSpectralPathProbe *p) {
    if(!w || w->pixels>=w->expected)return false;
    Vec3 v[COMPONENT_COUNT]={p->total,p->direct,p->indirect,p->volume,p->caustic,p->caustic_bounce,p->volume_caustic,
        p->volume_order[0],p->volume_order[1],p->volume_order[2],p->volume_order[3]};
    unsigned char encoded[COMPONENT_COUNT*3*8];size_t at=0;
    for(int k=0;k<COMPONENT_COUNT;++k) {
        double xyz[]={v[k].x,v[k].y,v[k].z};
        for(int c=0;c<3;++c) {
            uint64_t bits;memcpy(&bits,&xyz[c],sizeof(bits));
            for(int b=0;b<8;++b)encoded[at++]=(unsigned char)(bits>>(8*b));
        }
    }
    if(!bytes(w,encoded,sizeof(encoded)))return false;
    ++w->pixels;return true;
}
bool spectral_components_finish(SpectralComponentWriter *w,const char *path,bool overwrite,
                                RuntimeSpectralSurfacePathReadout *out) {
    bool ok=w && w->pixels==w->expected;
    if(w && w->file) {
        if(fflush(w->file)!=0 || ferror(w->file) || fsync(fileno(w->file))!=0)ok=false;
        if(fclose(w->file)!=0)ok=false;
        w->file=NULL;
    }
    if(ok) {
        /* link supplies atomic no-clobber semantics even if another publisher
         * creates the destination after ConfigureOutput. */
        ok=overwrite?rename(w->temporary,path)==0:link(w->temporary,path)==0;
        if(ok) {
            const char *name=strrchr(path,'/');name=name?name+1:path;
            snprintf(out->components_file,sizeof(out->components_file),"%s",name);
            ray_tracing_sha256_finish_hex(&w->hash,out->components_sha256);
            out->components_bytes=w->bytes;out->components_written=true;
        }
    }
    spectral_components_abort(w);return ok;
}
