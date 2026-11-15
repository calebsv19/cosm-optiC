/* Analytic sample-point KDE. No global fine lattice or expanded photon links.
 * Internal fragment owned by reconstruction.c; all state is render-context local. */
enum { KERNEL_LEAF=8, KERNEL_CACHE=16384, KERNEL_CANDIDATES=128 };
typedef struct {
    double radius, coefficient[SPECTRAL_MESH_PARTS], phase;
    uint64_t phase_key,generation;
    uint16_t region,parts;
    bool phase_ready;
} KernelPhoton;
typedef struct {
    Vec3 lo,hi;
    uint64_t bands[4];
    uint32_t left,right,first,count;
} KernelNode;
typedef struct {
    Vec3 position;
    uint32_t ids[KERNEL_CANDIDATES];
    int count;
    bool valid;
} KernelCandidates;
struct SpectralKernelState {
    KernelPhoton *photons;
    KernelNode *nodes;
    uint32_t *order;
    KernelCandidates *cache;
    uint32_t count;
};
static void kernel_free(SpectralReconstruction *r) {
    if (!r->kernel) return;
    free(r->kernel->photons);free(r->kernel->nodes);free(r->kernel->order);
    free(r->kernel->cache);free(r->kernel);
}
static void kernel_reset_phases(SpectralReconstruction *r) {
    if(r->kernel)for(int i=0;i<r->volume->count;i++)r->kernel->photons[i].phase_ready=false;
}
static bool kernel_less(const RuntimeSpectralVolume *v,uint32_t a,uint32_t b,int axis_id) {
    double x=axis(v->positions[a],axis_id),y=axis(v->positions[b],axis_id);
    return x<y || (x==y && a<b);
}
static void kernel_select(RuntimeSpectralVolume *v,uint32_t lo,uint32_t hi,uint32_t mid,int axis_id) {
    uint32_t *ids=v->reconstruction->kernel->order;
    while(hi-lo>1) {
        uint32_t pi=lo+(hi-lo)/2,pivot=ids[pi],at=lo;
        ids[pi]=ids[hi-1];ids[hi-1]=pivot;
        for(uint32_t i=lo;i+1<hi;i++)if(kernel_less(v,ids[i],pivot,axis_id)) {
            uint32_t tmp=ids[at];ids[at++]=ids[i];ids[i]=tmp;
        }
        ids[hi-1]=ids[at];ids[at]=pivot;
        if(mid==at)return;
        if(mid<at)hi=at;else lo=at+1;
    }
}
static uint32_t kernel_tree(RuntimeSpectralVolume *v,uint32_t lo,uint32_t hi) {
    struct SpectralKernelState *s=v->reconstruction->kernel;
    uint32_t id=s->count++;
    KernelNode *n=&s->nodes[id];
    n->lo=vec3(INFINITY,INFINITY,INFINITY);n->hi=vec3(-INFINITY,-INFINITY,-INFINITY);
    for(uint32_t j=lo;j<hi;j++) {
        uint32_t k=s->order[j];Vec3 p=v->positions[k];double h=s->photons[k].radius;
        n->lo=vec3(fmin(n->lo.x,p.x-h),fmin(n->lo.y,p.y-h),fmin(n->lo.z,p.z-h));
        n->hi=vec3(fmax(n->hi.x,p.x+h),fmax(n->hi.y,p.y+h),fmax(n->hi.z,p.z+h));
        int band=v->points[k].band;n->bands[band/64]|=UINT64_C(1)<<(band%64);
    }
    if(hi-lo<=KERNEL_LEAF) { n->first=lo;n->count=hi-lo;return id; }
    Vec3 size=vec3_sub(n->hi,n->lo);int a=size.x>=size.y&&size.x>=size.z?0:size.y>=size.z?1:2;
    uint32_t mid=lo+(hi-lo)/2;kernel_select(v,lo,hi,mid,a);
    n->left=kernel_tree(v,lo,mid);n->right=kernel_tree(v,mid,hi);return id;
}
static unsigned kernel_parts(const SpectralMeshBoundary *m,Vec3 p,double radius) {
    if(!m->count)return 1;
    unsigned parts=0;
    for(int j=0;j<m->count;j++) {
        SpectralMeshPlane f=m->plane[j];
        double d=vec3_dot(f.n,p)-f.d,margin=radius*(fabs(f.n.x)+fabs(f.n.y)+fabs(f.n.z));
        if(d>=margin)return 1; /* Entire kernel outside one supporting face. */
        if(d+margin>0)parts|=1u<<(j+1);
    }
    return parts;
}
static bool kernel_build(RuntimeSpectralVolume *v) {
    SpectralReconstruction *r=v->reconstruction;
    struct SpectralKernelState *s=r->kernel=spectral_reconstruction_resize(r,NULL,0,sizeof(*s));
    if(!s)return false;
    if(!v->count) { r->built=true;return true; }
    s->photons=spectral_reconstruction_resize(r,NULL,0,(size_t)v->count*sizeof(*s->photons));
    s->nodes=spectral_reconstruction_resize(r,NULL,0,(size_t)v->count*sizeof(*s->nodes));
    s->order=spectral_reconstruction_resize(r,NULL,0,(size_t)v->count*sizeof(*s->order));
    if(!s->photons || !s->nodes || !s->order)return false;
    for(int i=0;i<v->count;i++) {
        Vec3 p=v->positions[i];KernelPhoton *k=&s->photons[i];
        k->region=spectral_domain_region(&r->domain,p);
        if(!k->region)return spectral_reconstruction_fail(r,"kernel photon outside occupied domain");
        if((unsigned)v->points[i].band>=256)return spectral_reconstruction_fail(r,"kernel band index exceeds256");
        int count=0;
        for(int j=v->heads[spectral_volume_bucket(v->points[i].cell,v->points[i].band)];j;j=v->points[j-1].next)
            if(v->points[j-1].cell==v->points[i].cell && v->points[j-1].band==v->points[i].band && ++count>=8)break;
        k->radius=v->config.cell_m*(count>=8?1:2);
        if(count<8 && !v->points[i].first)k->radius=fmax(k->radius,v->config.higher_scatter_support_m);
        k->parts=(uint16_t)kernel_parts(&r->domain.mesh,p,k->radius);
        double weights[SPECTRAL_MESH_PARTS];
        if(!spectral_mesh_weights(&r->domain.mesh,p,weights))return spectral_reconstruction_fail(r,"kernel photon inside mesh");
        double left[3],peak[3],right[3];
        for(int a=0;a<3;a++) { peak[a]=axis(p,a);left[a]=peak[a]-k->radius;right[a]=peak[a]+k->radius; }
        for(int part=0;part<=r->domain.mesh.count;part++) {
            if(!(k->parts&(1u<<part)) || !(weights[part]>0))continue;
            double norm=spectral_domain_tent_norm(r,left,peak,right,k->region,part);
            if(r->error[0])return false;
            if(!(norm>0) || !isfinite(norm))return spectral_reconstruction_fail(r,"kernel occupied-domain normalization invalid");
            k->coefficient[part]=v->points[i].watts*weights[part]/norm;
        }
        s->order[i]=(uint32_t)i;
    }
    kernel_tree(v,0,(uint32_t)v->count);r->node_count=s->count;
    /* Optional exact-position, achromatic candidate lists. Overflow uses BVH. */
    size_t bytes=KERNEL_CACHE*sizeof(*s->cache);
    size_t transport=(size_t)v->capacity*(sizeof(SpectralCollision)+sizeof(Vec3))+65536*sizeof(int);
    if(bytes<=r->budget_bytes && r->owned_bytes+transport<=r->budget_bytes-bytes) {
        s->cache=calloc(KERNEL_CACHE,sizeof(*s->cache));
        if(s->cache) { r->owned_bytes+=bytes;r->peak_bytes=fmax(r->peak_bytes,r->owned_bytes+transport); }
    }
    r->built=true;return true;
}
static bool kernel_contains(const KernelNode *n,Vec3 p) {
    return p.x>n->lo.x && p.x<n->hi.x && p.y>n->lo.y && p.y<n->hi.y && p.z>n->lo.z && p.z<n->hi.z;
}
static bool kernel_hit(const RuntimeSpectralVolume *v,uint32_t i,Vec3 p) {
    Vec3 d=vec3_sub(p,v->positions[i]);double h=v->reconstruction->kernel->photons[i].radius;
    return fabs(d.x)<h && fabs(d.y)<h && fabs(d.z)<h;
}
static void kernel_candidates(RuntimeSpectralVolume *v,uint32_t id,Vec3 p,KernelCandidates *out) {
    const struct SpectralKernelState *s=v->reconstruction->kernel;const KernelNode *n=&s->nodes[id];
    if(!kernel_contains(n,p) || out->count<0)return;
    if(!n->count) { kernel_candidates(v,n->left,p,out);kernel_candidates(v,n->right,p,out);return; }
    for(uint32_t j=n->first;j<n->first+n->count;j++) {
        uint32_t i=s->order[j];
        if(!kernel_hit(v,i,p))continue;
        if(out->count==KERNEL_CANDIDATES) { out->count=-1;return; }
        out->ids[out->count++]=i;
    }
}
static double kernel_value(RuntimeSpectralVolume *v,uint32_t i,Vec3 p,uint16_t region,const double *weights,double *first) {
    SpectralReconstruction *r=v->reconstruction;KernelPhoton *k=&r->kernel->photons[i];
    if(k->region!=region)return 0;
    Vec3 delta=vec3_sub(p,v->positions[i]);double h=k->radius;
    double tent=fmax(0,1-fabs(delta.x)/h)*fmax(0,1-fabs(delta.y)/h)*fmax(0,1-fabs(delta.z)/h);
    if(!(tent>0))return 0;
    double flux=0;
    for(int j=0;j<=r->domain.mesh.count;j++)flux+=k->coefficient[j]*weights[j];
    if(!(flux>0))return 0;
    if(!k->phase_ready || (r->phase_key?k->phase_key!=r->phase_key:k->generation!=r->generation)) {
        double g=v->config.g,mu=fmax(-1,fmin(1,vec3_dot(v->points[i].incoming,r->phase_direction))),d=1+g*g-2*g*mu;
        k->phase=(1-g*g)/(4*M_PI*d*sqrt(d));k->phase_key=r->phase_key;k->generation=r->generation;k->phase_ready=true;
    }
    double value=flux*tent*k->phase;
    if(v->points[i].first)*first+=value;
    return value;
}
static void kernel_query(RuntimeSpectralVolume *v,uint32_t id,Vec3 p,int band,uint16_t region,const double *weights,double *first,double *sum) {
    const struct SpectralKernelState *s=v->reconstruction->kernel;const KernelNode *n=&s->nodes[id];
    if(!(n->bands[band/64]&(UINT64_C(1)<<(band%64))) || !kernel_contains(n,p))return;
    if(!n->count) {
        kernel_query(v,n->left,p,band,region,weights,first,sum);
        kernel_query(v,n->right,p,band,region,weights,first,sum);return;
    }
    for(uint32_t j=n->first;j<n->first+n->count;j++) {
        uint32_t i=s->order[j];if(v->points[i].band==band)*sum+=kernel_value(v,i,p,region,weights,first);
    }
}
static double kernel_source(RuntimeSpectralVolume *v,Vec3 p,int band,double *first) {
    SpectralReconstruction *r=v->reconstruction;
    if(!v->count || (unsigned)band>=256)return 0;
    uint16_t region=spectral_domain_region(&r->domain,p);double weights[SPECTRAL_MESH_PARTS];
    if(!region || !spectral_mesh_weights(&r->domain.mesh,p,weights))return 0;
    struct SpectralKernelState *s=r->kernel;
    KernelCandidates *c=NULL;
    if(s->cache && !v->config.disable_camera_cache) {
        uint64_t bits[3];memcpy(bits,&p,sizeof(bits));
        c=&s->cache[hash(bits[0]^hash(bits[1])^hash(bits[2]))&(KERNEL_CACHE-1)];
        if(!c->valid || memcmp(&c->position,&p,sizeof(p))) {
            c->valid=true;c->position=p;c->count=0;kernel_candidates(v,0,p,c);r->exact_queries++;
        } else r->cached_queries++;
    }
    double sum=0;
    if(!c || c->count<0) { kernel_query(v,0,p,band,region,weights,first,&sum);return sum; }
    for(int j=0;j<c->count;j++) { uint32_t i=c->ids[j];if(v->points[i].band==band)sum+=kernel_value(v,i,p,region,weights,first); }
    return sum;
}
