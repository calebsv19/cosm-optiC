/* Independent tent-kernel oracle at interior and clipped boundary nodes. */
static void independent_support(void) {
  for (int adaptive=0;adaptive<2;adaptive++)
  for (int boundary=0;boundary<2;boundary++) {
    RuntimeSpectralVolume *v=volume(adaptive ? SPECTRAL_RECONSTRUCTION_ADAPTIVE : SPECTRAL_RECONSTRUCTION_CONTINUOUS);
    v->config.reconstruction_cell_m=.05;
    RuntimeScene3D scene={0};char diag[192];
    assert(RuntimeSpectralVolume_BindScene(v,&scene,diag,sizeof(diag)));
    Vec3 p=boundary?vec3(0,.4,.4):vec3(.4,.4,.4);
    event(v,p);
    Ray3D ray={.origin={.45,.45,1},.direction={0,0,-1}};
    assert(spectral_reconstruction_begin_ray(v,ray));
    double radius=adaptive?.2:.1;
    double expected=(boundary?2:1)/(4*M_PI*radius*radius*radius);
    assert(fabs(sample(v,p)-expected)<expected*1e-10);
    size_t capacity=v->reconstruction->support_cache_size;
    double reference[64];
    struct SpectralSupportCell *cells=v->reconstruction->support_cells;
    assert(cells);
    void *bitmap0=v->reconstruction->support_bitmap[0],*bitmap1=v->reconstruction->support_bitmap[1];
    assert(bitmap0);
    v->reconstruction->support_bitmap[0]=v->reconstruction->support_bitmap[1]=NULL;
    v->reconstruction->support_cells=NULL;
    v->reconstruction->support_cache_size=1024;
    v->config.disable_camera_cache=true;
    for(int k=0;k<64;k++) reference[k]=sample(v,vec3(p.x+k*.001,p.y,p.z));
    v->reconstruction->support_cache_size=capacity;
    v->reconstruction->support_cells=cells;
    v->reconstruction->support_bitmap[0]=bitmap0;v->reconstruction->support_bitmap[1]=bitmap1;
    v->config.disable_camera_cache=false;
    for(int k=0;k<64;k++) {
      double value=sample(v,vec3(p.x+k*.001,p.y,p.z));
      assert(!memcmp(&value,&reference[k],sizeof(value)));
    }
    double tail=sample(v,vec3(p.x+radius+.000001,p.y,p.z));
    fprintf(stderr,"independent tail adaptive=%d boundary=%d value=%.17g\n",adaptive,boundary,tail);
    assert(fabs(tail)<expected*1e-12);
    assert(v->positions && v->reconstruction->link_count==0);
    assert(v->reconstruction->owned_bytes<256u*1024u*1024u);
    double integral=0;
    for (int z=0;z<=20;z++) for (int y=0;y<=20;y++) for (int x=0;x<=20;x++) {
      double weight=(x==0||x==20?.5:1)*(y==0||y==20?.5:1)*(z==0||z==20?.5:1);
      integral+=weight*sample(v,vec3(x*.05,y*.05,z*.05))*.000125;
    }
    fprintf(stderr,"independent integral adaptive=%d boundary=%d measured=%.17g expected=%.17g delta=%.17g\n",adaptive,boundary,integral,1/(4*M_PI),integral-1/(4*M_PI));
    assert(fabs(integral-1/(4*M_PI))<1e-10);
    RuntimeSpectralVolume_Free(v);
  }
  {
    RuntimeSpectralVolume *v=volume(SPECTRAL_RECONSTRUCTION_CONTINUOUS);
    v->config.reconstruction_cell_m=.05;
    RuntimePrimitive3D plane;RuntimeScene3D scene=plane_scene(&plane,.517,2);
    bind(v,&scene);event(v,vec3(.475,.4,.4));begin(v);
    assert(sample(v,vec3(.55,.4,.4))==0);
    assert(fabs(integral(v,.517)-1)<1e-10);
    RuntimeSpectralVolume_Free(v);
  }
  {
    RuntimeSpectralVolume *v=volume(SPECTRAL_RECONSTRUCTION_CONTINUOUS);
    v->config.reconstruction_cell_m=.05;
    RuntimeScene3D scene={0};bind(v,&scene);begin(v);
    assert(sample(v,vec3(.5,.5,.5))==0);
    assert(!v->reconstruction->error[0]);
    RuntimeSpectralVolume_Free(v);
  }
  {
    RuntimeSpectralVolume *v=volume(SPECTRAL_RECONSTRUCTION_CONTINUOUS);
    v->config.reconstruction_cell_m=.05;v->config.g=.35;v->config.phase_tolerance=0;
    RuntimeScene3D scene={0};bind(v,&scene);event(v,vec3(.4,.4,.4));begin(v);
    double g=v->config.g;
    double forward=(1+g)/(4*M_PI*(1-g)*(1-g)*.001);
    assert(fabs(sample(v,vec3(.4,.4,.4))-forward)<forward*1e-10);
    Ray3D reversed=RuntimeRay3D_Make(vec3(.5,.5,-3),vec3(0,0,1));
    v->reconstruction->generation=UINT64_MAX;
    assert(spectral_reconstruction_begin_ray(v,reversed));
    double backward=(1-g)/(4*M_PI*(1+g)*(1+g)*.001);
    assert(fabs(sample(v,vec3(.4,.4,.4))-backward)<backward*1e-10);
    assert(fabs(sample(v,vec3(.4,.4,.4))-backward)<backward*1e-10);
    RuntimeSpectralVolume_Free(v);
  }
  {
    RuntimeSpectralVolume *v=volume(SPECTRAL_RECONSTRUCTION_CONTINUOUS);
    v->config.reconstruction_cell_m=.05;
    RuntimeScene3D scene={0};bind(v,&scene);event(v,vec3(.4,.4,.4));
    v->reconstruction->budget_bytes=1024*1024;
    begin(v);assert(!v->reconstruction->support_cells);
    double expected=1/(4*M_PI*.001);
    assert(fabs(sample(v,vec3(.4,.4,.4))-expected)<expected*1e-10);
    RuntimeSpectralVolume_Free(v);
  }
  {
    RuntimeSpectralVolume *v=volume(SPECTRAL_RECONSTRUCTION_CONTINUOUS);
    v->config.reconstruction_cell_m=.05;
    RuntimeScene3D scene={0};bind(v,&scene);event(v,vec3(.4,.4,.4));begin(v);
    SpectralReconstruction *r=v->reconstruction;
    size_t transport=(size_t)v->capacity*(sizeof(SpectralCollision)+sizeof(Vec3))+65536*sizeof(int);
    r->budget_bytes=r->owned_bytes+transport+SPECTRAL_SOURCE_CACHE_SIZE*sizeof(SpectralSourceStencil)-1;
    double expected=1/(4*M_PI*.001);
    assert(fabs(sample(v,vec3(.4,.4,.4))-expected)<expected*1e-10);
    assert(!r->source_cache);
    RuntimeSpectralMapStats stats={0};spectral_reconstruction_stats(v,&stats);
    assert(stats.reconstruction_bytes<=r->budget_bytes);
    assert(stats.reconstruction_peak_bytes<=r->budget_bytes);
    RuntimeSpectralVolume_Free(v);
  }
  for (int high=0;high<2;high++) {
    int band=high?100:40;
    RuntimeSpectralVolume *v=volume(SPECTRAL_RECONSTRUCTION_CONTINUOUS);
    v->config.reconstruction_cell_m=.05;
    RuntimeScene3D scene={0};bind(v,&scene);event(v,vec3(.4,.4,.4));
    uint64_t cell=v->points[0].cell;
    v->heads[spectral_volume_bucket(cell,0)]=0;
    v->points[0].band=band;v->heads[spectral_volume_bucket(cell,band)]=1;
    begin(v);double first=0,expected=1/(4*M_PI*.001);
    double value=spectral_reconstruction_source(v,vec3(.4,.4,.4),band,&first);
    assert(fabs(value-expected)<expected*1e-10);
    begin(v);assert(sample(v,vec3(.4,.4,.4))==0);
    if(high)assert(!v->reconstruction->support_bitmap[0]);
    else assert(v->reconstruction->support_bitmap_word_bytes==8);
    RuntimeSpectralVolume_Free(v);
  }
  puts("PASS independent support: analytic interior/boundary density, finite support, retained buckets, no expanded links");
}
