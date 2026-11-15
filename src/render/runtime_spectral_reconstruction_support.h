#if defined(__GNUC__) || defined(__clang__)
#define SPECTRAL_SUPPORT_NOINLINE __attribute__((noinline))
#else
#define SPECTRAL_SUPPORT_NOINLINE
#endif
enum { SUPPORT_CACHE_SIZE=4194304 };
struct SpectralSupportCell { uint64_t key;uint32_t head; };
static uint64_t support_cell_key(uint64_t cell,int band,int level) {
    return 1+cell+((uint64_t)band<<36)+((uint64_t)level<<44);
}
struct SpectralSupportCache {
    uint64_t key, phase_key, generation;
    double total, first;
    bool empty; /* No positive kernel flux: independent of viewing direction. */
};
/* Opt-in finer evaluation lattice with unchanged photon averaging support.
 * Nodes retain no expanded photon links: directional values gather from the
 * original collision buckets and use the existing bounded angular cache.
 * Internal implementation fragment included only by reconstruction.c. */
static bool support_independent(const RuntimeSpectralVolume *v) {
    return v->config.reconstruction_cell_m > 0 &&
           v->config.reconstruction_cell_m != v->config.cell_m;
}
static double support_weight(double x, double peak, double radius) {
    return fmax(0,1-fabs(x-peak)/radius);
}
/* Optional conservative spectral occupancy. It stores no radiance values. */
static void support_build_occupancy(RuntimeSpectralVolume *v) {
    SpectralReconstruction *r=v->reconstruction;if(!v->count)return;
    int max_band=0;
    for(int i=0;i<v->count;i++) if(v->points[i].band>max_band)max_band=v->points[i].band;
    if(max_band>=64)return;
    unsigned word=max_band<32?4:8;
    int levels=v->config.reconstruction==SPECTRAL_RECONSTRUCTION_ADAPTIVE?2:1;
    size_t bytes[2]={0},total=0;
    for(int level=0;level<levels;level++) {
        double spacing=spectral_reconstruction_spacing(v)*(level?2:1);
        uint64_t count=1;
        for(int a=0;a<3;a++) {
            r->support_bitmap_dims[level][a]=(size_t)ceil((axis(v->hi,a)-axis(v->lo,a))/spacing)+1;
            count*=r->support_bitmap_dims[level][a];
        }
        if(count>r->budget_bytes/word)return;
        bytes[level]=(size_t)count*word;
        if(total>r->budget_bytes-bytes[level])return;
        total+=bytes[level];
    }
    size_t transport=(size_t)v->capacity*(sizeof(SpectralCollision)+sizeof(Vec3))+65536*sizeof(int);
    size_t reserve=65536*sizeof(struct SpectralSupportCache)+SPECTRAL_SOURCE_CACHE_SIZE*sizeof(SpectralSourceStencil);
    if(total+reserve>r->budget_bytes || r->owned_bytes+transport>r->budget_bytes-total-reserve)return;
    void *data[2]={0};size_t allocated=0;
    for(int level=0;level<levels;level++) {
        data[level]=calloc(1,bytes[level]);
        if(!data[level]) { free(data[0]);free(data[1]);return; }
        allocated+=bytes[level];r->peak_bytes=fmax(r->peak_bytes,r->owned_bytes+transport+allocated);
    }
    r->support_bitmap[0]=data[0];r->support_bitmap[1]=data[1];
    r->support_bitmap_word_bytes=word;r->owned_bytes+=total;
    for(int i=0;i<v->count;i++) {
        if(!(r->support_flux[i]>0))continue;
        int level=r->support_levels[i],low[3],high[3];
        double spacing=spectral_reconstruction_spacing(v)*(level?2:1),radius=v->config.cell_m*(level?2:1);
        Vec3 pos=v->positions[i];
        for(int a=0;a<3;a++) {
            double lo=axis(v->lo,a),x=axis(pos,a);
            low[a]=(int)fmax(0,floor((x-radius-lo)/spacing));
            high[a]=(int)fmin(r->support_bitmap_dims[level][a]-1,ceil((x+radius-lo)/spacing));
        }
        for(int z=low[2];z<=high[2];z++)for(int y=low[1];y<=high[1];y++)for(int x=low[0];x<=high[0];x++) {
            int ids[3]={x,y,z};bool occupied=true;
            for(int a=0;a<3;a++) {
                double peak=fmin(axis(v->hi,a),axis(v->lo,a)+ids[a]*spacing);
                if(!(support_weight(axis(pos,a),peak,radius)>0))occupied=false;
            }
            if(!occupied)continue;
            size_t at=((size_t)z*r->support_bitmap_dims[level][1]+y)*r->support_bitmap_dims[level][0]+x;
            if(word==4)((uint32_t *)data[level])[at]|=UINT32_C(1)<<v->points[i].band;
            else ((uint64_t *)data[level])[at]|=UINT64_C(1)<<v->points[i].band;
        }
    }
}
static bool support_occupied(const SpectralReconstruction *r,uint64_t key,int band) {
    uint64_t raw=key-1;int level=(int)((raw>>44)&1);
    if(!r->support_bitmap[level])return true;
    if((unsigned)band>=r->support_bitmap_word_bytes*8)return false;
    size_t x=(raw>>24)&4095,y=(raw>>12)&4095,z=raw&4095;
    size_t at=(z*r->support_bitmap_dims[level][1]+y)*r->support_bitmap_dims[level][0]+x;
    return r->support_bitmap_word_bytes==4
      ? ((((uint32_t *)r->support_bitmap[level])[at]>>band)&1)!=0
      : ((((uint64_t *)r->support_bitmap[level])[at]>>band)&1)!=0;
}
/* Normalize each photon's interpolated kernel over its occupied component.
 * Integration of the fine linear basis is exact, including clipped end cells.
 * This preserves deposited energy at interior points and boundaries. */
static bool support_prepare(RuntimeSpectralVolume *v) {
    SpectralReconstruction *r=v->reconstruction;
    for (int region=1;region<=r->domain.region_count;region++)
        if (!r->domain.regions[region].rectangular)
            return spectral_reconstruction_fail(r,"independent support requires rectangular occupied components");
    if (v->count) {
        r->support_flux=spectral_reconstruction_resize(r,NULL,0,(size_t)v->count*sizeof(double));
        r->support_levels=spectral_reconstruction_resize(r,NULL,0,(size_t)v->count*sizeof(uint8_t));
        if (!r->support_flux || !r->support_levels) return false;
    }
    for (int i=0;i<v->count;i++) {
        Vec3 pos=v->positions[i];
        uint16_t region=spectral_domain_region(&r->domain,pos);
        if (!region) return spectral_reconstruction_fail(r,"scattering event outside occupied component");
        int level=level_for(v,&v->points[i]);r->support_levels[i]=(uint8_t)level;
        double spacing=spectral_reconstruction_spacing(v)*(level?2:1);
        double radius=v->config.cell_m*(level?2:1),norm=1;
        const SpectralDomainRegion *c=&r->domain.regions[region];
        for (int a=0;a<3;a++) {
            double lo=axis(v->lo,a),hi=axis(v->hi,a),x=axis(pos,a),sum=0;
            int last=(int)ceil((hi-lo)/spacing);
            int begin=(int)fmax(0,floor((x-radius-lo)/spacing));
            int end=(int)fmin(last,ceil((x+radius-lo)/spacing));
            for (int j=begin;j<=end;j++) {
                double peak=fmin(hi,lo+j*spacing);
                double left=j?lo+(j-1)*spacing:lo,right=fmin(hi,peak+spacing);
                double volume=spectral_reconstruction_hat_integral(left,peak,right,axis(c->lo,a),axis(c->hi,a));
                sum+=support_weight(x,peak,radius)*volume;
            }
            norm*=sum;
        }
        if (!(norm>0) || !isfinite(norm)) return spectral_reconstruction_fail(r,"independent support normalization invalid");
        r->support_flux[i]=v->points[i].watts/norm;
    }
    /* Optional exact cell index, preserving descending insertion order. */
    if (v->count) {
        size_t capacity=1024;
        while(capacity<(size_t)v->count*2) capacity*=2;
        size_t bytes=capacity*sizeof(*r->support_cells)+(size_t)v->count*sizeof(*r->support_next);
        size_t transport=(size_t)v->capacity*(sizeof(SpectralCollision)+sizeof(Vec3))+
                         (v->heads?65536*sizeof(int):0);
        if(bytes+65536*sizeof(struct SpectralSupportCache)<=r->budget_bytes &&
           r->owned_bytes+transport<=r->budget_bytes-bytes-65536*sizeof(struct SpectralSupportCache)) {
            r->support_cells=spectral_reconstruction_resize(r,NULL,0,capacity*sizeof(*r->support_cells));
            r->support_next=spectral_reconstruction_resize(r,NULL,0,(size_t)v->count*sizeof(*r->support_next));
            if(!r->support_cells || !r->support_next) return false;
            r->support_cell_capacity=capacity;
            for(int i=0;i<v->count;i++) {
                uint64_t key=support_cell_key(v->points[i].cell,v->points[i].band,r->support_levels[i]);
                size_t at=hash(key)&(capacity-1);
                while(r->support_cells[at].key && r->support_cells[at].key!=key)at=(at+1)&(capacity-1);
                r->support_cells[at].key=key;r->support_next[i]=r->support_cells[at].head;
                r->support_cells[at].head=(uint32_t)i+1;
            }
        }
    }
    support_build_occupancy(v);
    return true;
}
static SPECTRAL_SUPPORT_NOINLINE double support_exact_node(const SpectralReconstruction *r,
                                  const SpectralReconstructionNode *n,
                                  Vec3 outgoing, double *first, bool *empty) {
    RuntimeSpectralVolume *v=r->volume;
    *empty=true;
    uint64_t key=n->key-1;
    int ids[3]={(int)((key>>24)&4095),(int)((key>>12)&4095),(int)(key&4095)};
    int band=(int)((key>>36)&255),level=(int)((key>>44)&1);
    uint16_t region=(uint16_t)(key>>45);
    double spacing=spectral_reconstruction_spacing(v)*(level?2:1);
    double radius=v->config.cell_m*(level?2:1),left[3],peak[3],right[3];
    int low[3],high[3];
    for (int a=0;a<3;a++) {
        double lo=axis(v->lo,a),hi=axis(v->hi,a);
        peak[a]=fmin(hi,lo+ids[a]*spacing);
        left[a]=fmax(lo,peak[a]-radius);right[a]=fmin(hi,peak[a]+radius);
        low[a]=(int)fmax(0,floor((left[a]-lo)/v->config.cell_m));
        high[a]=(int)fmin(v->dims[a]-1,floor((right[a]-lo)/v->config.cell_m));
    }
    double total=0,g=v->config.g;
    for (int z=low[2];z<=high[2];z++)
        for (int y=low[1];y<=high[1];y++)
            for (int x=low[0];x<=high[0];x++) {
                uint64_t cell=((uint64_t)x<<24)|((uint64_t)y<<12)|(uint64_t)z;
                uint32_t head;
                if(r->support_cells) {
                    uint64_t key=support_cell_key(cell,band,level);
                    size_t at=hash(key)&(r->support_cell_capacity-1);
                    while(r->support_cells[at].key && r->support_cells[at].key!=key)
                        at=(at+1)&(r->support_cell_capacity-1);
                    head=r->support_cells[at].head;
                } else head=(uint32_t)v->heads[spectral_volume_bucket(cell,band)];
                for (uint32_t k=head;k;k=r->support_cells?r->support_next[k-1]:(uint32_t)v->points[k-1].next) {
                    const SpectralCollision *p=&v->points[k-1];
                    if (p->cell!=cell || p->band!=band || r->support_levels[k-1]!=level) continue;
                    Vec3 pos=v->positions[k-1];
                    if (spectral_domain_region(&r->domain,pos)!=region) continue;
                    double w=1;
                    for (int a=0;a<3;a++) w*=support_weight(axis(pos,a),peak[a],radius);
                    if (!(w>0)) continue;
                    if (r->support_flux[k-1]>0) *empty=false;
                    double mu=fmax(-1,fmin(1,vec3_dot(p->incoming,outgoing)));
                    double d=1+g*g-2*g*mu;
                    double value=r->support_flux[k-1]*w*(1-g*g)/(4*M_PI*d*sqrt(d));
                    total+=value;if(p->first)*first+=value;
                }
            }
    return total;
}

/* Reuse the legacy ordered, exact-position achromatic stencil cache. */
static void source_prepare(RuntimeSpectralVolume *,Vec3,SpectralSourceStencil *);
static bool source_cache_ready(SpectralReconstruction *);
static SPECTRAL_SUPPORT_NOINLINE double support_source(RuntimeSpectralVolume *v, Vec3 p, int band, double *first) {
    SpectralReconstruction *r=v->reconstruction;
    if (!v->count) return 0;
    SpectralSourceStencil local,*stencil=&local;
    if (!v->config.disable_camera_cache && source_cache_ready(r) &&
        r->source_cursor<SPECTRAL_SOURCE_CACHE_SIZE) {
        size_t index=r->source_cursor++;
        stencil=&r->source_cache[index];
        if(index>=r->source_cached_count || memcmp(&stencil->position,&p,sizeof(p))) {
            r->source_misses++;source_prepare(v,p,stencil);
            if(index>=r->source_cached_count)r->source_cached_count=index+1;
        } else r->source_hits++;
    } else {
        if(r->source_cursor>=SPECTRAL_SOURCE_CACHE_SIZE)r->source_overflows++;
        source_prepare(v,p,stencil);
    }
    double total=0;
    for(int group=0;group<stencil->count;group++) {
        const SpectralSourceGroup *g=&stencil->groups[group];
        for(int i=0;i<8;i++) {
            double weight=g->weights[i];if(!(weight>0))continue;
            uint64_t offset=((uint64_t)(i&1)<<24)|((uint64_t)((i>>1)&1)<<12)|(uint64_t)((i>>2)&1);
            uint64_t key=g->cell_key+offset+((uint64_t)band<<36);
            if(!support_occupied(r,key,band))continue;
            struct SpectralSupportCache *c=&r->support_cache[hash(key)&(r->support_cache_size-1)];
            if(c->key!=key || (!c->empty && (r->phase_key ? c->phase_key!=r->phase_key : c->generation!=r->generation))) {
                SpectralReconstructionNode node={.key=key};
                c->first=0;c->total=support_exact_node(r,&node,r->phase_direction,&c->first,&c->empty);
                c->key=key;c->phase_key=r->phase_key;c->generation=r->generation;
                r->exact_queries++;
            } else r->cached_queries++;
            total+=weight*c->total;*first+=weight*c->first;
        }
    }
    return total;
}

#undef SPECTRAL_SUPPORT_NOINLINE
