/* Optional sample-point kNN bandwidths for scattered receiver deposits.
 * Unscattered caustic records retain the original radius and fine index. */
struct SpectralWideIndex {
    int *heads;
    struct WideItem { int next;int64_t u,v; } *items;
    double radius;
};
static void wide_free(RuntimeSpectralSurfaceMap *m) {
    if(m->wide) { free(m->wide->heads);free(m->wide->items);free(m->wide); }
}
bool spectral_surface_wide_prepare(RuntimeSpectralSurfaceMap *m,const RuntimeSpectralPencil *p,
                         const RuntimeScene3D *scene,int *expanded) {
    *expanded=0;
    struct SpectralWideIndex *w=m->wide=calloc(1,sizeof(*w));if(!w)return false;
    w->radius=p->map.scattered_support_m;
    w->heads=malloc((m->mask+1)*sizeof(int));
    if(m->count)w->items=calloc((size_t)m->count,sizeof(*w->items));
    if(!w->heads || (m->count&&!w->items))return false;
    for(size_t j=0;j<=m->mask;j++)w->heads[j]=-1;
    for(int i=0;i<m->count;i++) {
        Deposit *d=&m->records[i];if(!d->scattered)continue;
        struct WideItem *entry=&w->items[i];
        if(!spectral_surface_cell(&scene->primitives[d->primitive].shape.plane,d->position,w->radius,&entry->u,&entry->v))return false;
        size_t b=spectral_surface_bucket(m,d->primitive,d->band,entry->u,entry->v);entry->next=w->heads[b];w->heads[b]=i;
    }
    RuntimeSceneAcceleration3DInstanceBounds *bounds=NULL;int count=0;
    if(!RuntimeSceneAcceleration3D_CaptureInstanceBounds(scene,&bounds,&count,NULL))return false;
    for(int i=0;i<scene->primitiveCount;i++)if(scene->primitives[i].kind!=RUNTIME_PRIMITIVE_3D_KIND_PLANE) {
        bool found=false;for(int j=0;j<count;j++)if(bounds[j].primitiveIndex==i)found=true;
        if(!found) { RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(bounds,count);free(bounds);return false; }
    }
    int target=p->map.scattered_neighbors?p->map.scattered_neighbors:16;
    for(int i=0;i<m->count;i++) {
        Deposit *d=&m->records[i];if(!d->scattered)continue;
        const RuntimePlane3D *plane=&scene->primitives[d->primitive].shape.plane;
        Vec3 delta=vec3_sub(d->position,plane->origin);
        double cap=fmin(w->radius,fmin(plane->width/2-fabs(vec3_dot(delta,plane->axisU)),plane->height/2-fabs(vec3_dot(delta,plane->axisV))));
        while(cap>p->receiver_radius_m && !clear_of_other_surfaces(d,scene,bounds,count,cap))cap*=.5;
        if(cap<=p->receiver_radius_m)continue;
        double nearest[64];int n=0;struct WideItem entry=w->items[i];
        for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++) {
            size_t b=spectral_surface_bucket(m,d->primitive,d->band,entry.u+x,entry.v+y);
            for(int j=w->heads[b];j>=0;j=w->items[j].next) {
                const Deposit *q=&m->records[j];
                if(w->items[j].u!=entry.u+x || w->items[j].v!=entry.v+y || q->primitive!=d->primitive || q->band!=d->band || q->front!=d->front)continue;
                Vec3 dist=vec3_sub(q->position,d->position);double d2=vec3_dot(dist,dist);
                if(d2>=cap*cap || (n==target && d2>=nearest[n-1]))continue;
                int at=n<target?n++:n-1;
                while(at>0 && nearest[at-1]>d2) { nearest[at]=nearest[at-1];at--; }
                nearest[at]=d2;
            }
        }
        double radius=n<target?cap:fmax(p->receiver_radius_m,sqrt(nearest[n-1]));
        d->scale=(float)(radius/p->receiver_radius_m);
        if(d->scale*p->receiver_radius_m>cap)d->scale=nextafterf(d->scale,0);
        if(d->scale<1)d->scale=1;
        *expanded+=d->scale>1;
    }
    RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(bounds,count);free(bounds);return true;
}
static void probe_deposit(RuntimeSpectralDepositProbe *probe, const Deposit *d, int record,
                          double support, double kernel, double radiance) {
    if (probe && radiance > probe->radiance)
        *probe = (RuntimeSpectralDepositProbe){.radiance=radiance, .deposited_watts=d->watts,
            .vacuum_nm=d->vacuum_nm, .support_m=support, .kernel_per_m2=kernel, .incident=d->incident,
            .record=record, .source=d->source, .scattered=d->scattered};
}
static void deposit_color(RuntimeSpectralDepositColor *color, const Deposit *deposit, double radiance) {
    if (!color || radiance <= 0) return;
    color->radiance += radiance;
    color->xyz = vec3_add(color->xyz, vec3_scale(RuntimeSpectral_CIE1931(deposit->vacuum_nm), radiance));
}

static double wide_radiance(const RuntimeSpectralSurfaceMap *m,const RuntimeSpectralPencil *p,
                            const RuntimeScene3D *scene,const RuntimeSpectralPathReadout *hit,int band,
                            unsigned long long *candidates, RuntimeSpectralDepositProbe *probe,
                            RuntimeSpectralDepositColor *color) {
    const struct SpectralWideIndex *w=m->wide;int64_t u,v;
    if(!spectral_surface_cell(&scene->primitives[hit->primitive].shape.plane,hit->position,w->radius,&u,&v))return 0;
    double sum=0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++) {
        size_t b=spectral_surface_bucket(m,hit->primitive,band,u+x,v+y);
        for(int j=w->heads[b];j>=0;j=w->items[j].next) {
            const Deposit *d=&m->records[j];
            if(w->items[j].u!=u+x || w->items[j].v!=v+y || d->primitive!=hit->primitive || d->band!=band || d->front!=hit->receiver_front)continue;
            ++*candidates;
            double distance=vec3_length(vec3_sub(d->position,hit->position));
            double support=d->scale*p->receiver_radius_m;
            double kernel=RuntimeCausticPhotonSurfaceKernel3D_Weight(distance,support);
            double value=d->watts*kernel/M_PI;
            sum+=value;
            probe_deposit(probe,d,j,support,kernel,value);
            deposit_color(color,d,value);
        }
    }
    return sum;
}
