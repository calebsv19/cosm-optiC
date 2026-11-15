/* Analytic kernel checks use independent composite quadrature and HG formulas. */
static RuntimeSpectralVolume *kernel_volume(double h) {
    RuntimeSpectralVolumeConfig cfg={.kind=SPECTRAL_VOLUME_BOX,.reconstruction=SPECTRAL_RECONSTRUCTION_KERNEL,
        .cell_m=h,.phase_tolerance=0,.lo={0,0,0},.hi={1,1,1}};
    char diag[192];RuntimeSpectralVolume *v=RuntimeSpectralVolume_Load(&cfg,diag,sizeof(diag));assert(v);return v;
}
static void kernel_cases(void) {
    RuntimeScene3D empty={0};
    for(int boundary=0;boundary<2;boundary++) {
        RuntimeSpectralVolume *v=kernel_volume(.1);bind(v,&empty);
        Vec3 p=boundary?vec3(.025,.5,.5):vec3(.5,.5,.5);event(v,p);begin(v);
        double actual=mesh_integral_oracle(v);printf("kernel rectangle boundary=%d mass=%.12g\n",boundary,actual);assert(fabs(actual-1)<1e-10);
        double saved=sample(v,p);v->config.local_refinement_step_m=.00025;v->config.local_refinement_lo=vec3(.2,.2,.2);v->config.local_refinement_hi=vec3(.8,.8,.8);
        assert(sample(v,p)==saved);assert(sample(v,vec3(-.001,.5,.5))==0);
        RuntimeSpectralVolume_Free(v);
    }
    RuntimePrimitive3D primitive;RuntimeTriangle3D triangles[12];RuntimeScene3D mesh=mesh_cube(&primitive,triangles);
    Vec3 points[]={{.425,.55,.55},{.425,.425,.55},{.425,.425,.425}};
    for(int i=0;i<3;i++) {
        RuntimeSpectralVolume *v=kernel_volume(.2);bind(v,&mesh);event(v,points[i]);begin(v);
        double total=mesh_integral_oracle(v);printf("kernel mesh face/edge/corner=%d mass=%.12g\n",i,total);fflush(stdout);assert(fabs(total-1)<2e-4);
        assert(sample(v,vec3(.55,.55,.55))==0);assert(sample(v,vec3(.66,.55,.55))==0);
        v->config.disable_camera_cache=true;double scalar=sample(v,points[i]);v->config.disable_camera_cache=false;assert(sample(v,points[i])==scalar);
        RuntimeSpectralVolume_Free(v);
    }
    {
        RuntimeScene3D prism=mesh_prism(&primitive,triangles);
        Vec3 deposits[]={{.55,.55,.5},{.55,.55,.35},{.65,.35,.35}};
        for(int i=0;i<3;i++) {
            RuntimeSpectralVolume *v=kernel_volume(.1);bind(v,&prism);event(v,deposits[i]);begin(v);
            double total=prism_oracle(v);printf("kernel oblique prism case=%d mass=%.12g\n",i,total);fflush(stdout);assert(fabs(total-1)<2e-4);
            assert(sample(v,vec3(.45,.45,.5))==0);
            if(i==0){assert(sample(v,vec3(.399,.5,.5))==0);assert(sample(v,vec3(.5,.399,.5))==0);}
            RuntimeSpectralVolume_Free(v);
        }
    }
    /* Wider sparse higher-order kernel is still normalized; first events stay narrow. */
    for(int higher=0;higher<2;higher++) {
        RuntimeSpectralVolume *v=kernel_volume(.05);v->config.higher_scatter_support_m=.2;bind(v,&empty);event(v,vec3(.5,.5,.5));v->points[0].first=!higher;begin(v);
        assert(fabs(mesh_integral_oracle(v)-1)<1e-10);
        assert((sample(v,vec3(.65,.5,.5))>0)==(higher!=0));RuntimeSpectralVolume_Free(v);
    }
    /* Independent one-dimensional camera integral through an off-grid tent. */
    for(int k=0;k<3;k++) {
        double st=k==0?.01:k==1?1:100;
        RuntimeSpectralVolume *v=kernel_volume(.1);
        v->config.local_refinement_step_m=.00025;v->config.local_refinement_lo=vec3(.05,.1,.1);v->config.local_refinement_hi=vec3(.95,.9,.9);
        v->config.absorption=(RuntimeSpectralCurve){.count=2,.points={{380,0},{780,0}}};
        v->config.scattering=(RuntimeSpectralCurve){.count=2,.points={{380,st},{780,st}}};
        bind(v,&empty);Vec3 pos=vec3(.45317,.451,.457);event(v,pos);
        Ray3D ray=RuntimeRay3D_Make(vec3(-.1,pos.y,pos.z),vec3(1,0,0));RuntimeSpectralPencilLine line={.vacuum_nm=550,.band=0};RuntimeSpectralPathReadout out={.weight=1};bool restart=false;
        assert(RuntimeSpectralVolume_Segment(v,&line,&ray,2,true,0,&out,&restart));
        double expected=0;int n=200000;
        for(int i=0;i<n;i++){double x=(i+.5)/n;expected+=fmax(0,1-fabs(x-pos.x)/.2)/(.008*4*M_PI)*exp(-st*x)/n;}
        double error=fabs(out.inscattered-expected)/expected;
        printf("kernel local camera tau=%.2g relative=%.12g\n",st,error);assert(error<1e-5);
        assert(fabs(out.weight-exp(-st))<1e-12);RuntimeSpectralVolume_Free(v);
    }
    /* Camera quadrature beside a mesh edge, with an independent dense line integral. */
    for(int k=0;k<3;k++) {
        RuntimeScene3D cube=mesh_cube(&primitive,triangles);double st=k==0?.01:k==1?1:100;
        RuntimeSpectralVolume *v=kernel_volume(.1);v->config.absorption=(RuntimeSpectralCurve){.count=2,.points={{380,0},{780,0}}};v->config.scattering=(RuntimeSpectralCurve){.count=2,.points={{380,st},{780,st}}};
        bind(v,&cube);event(v,vec3(.449,.449,.53));Ray3D ray=RuntimeRay3D_Make(vec3(.44,.2,.53),vec3(0,1,0));RuntimeSpectralPencilLine line={.vacuum_nm=550,.band=0};RuntimeSpectralPathReadout out={.weight=1};bool restart=false;
        assert(RuntimeSpectralVolume_Segment(v,&line,&ray,.6,true,0,&out,&restart));double expected=0;int n=200000;
        for(int i=0;i<n;i++){double d=(i+.5)*.6/n;expected+=sample(v,vec3(.44,.2+d,.53))*exp(-st*d)*.6/n;}
        double error=fabs(out.inscattered-expected)/expected;printf("kernel mesh camera tau=%.2g relative=%.12g\n",st,error);assert(error<2e-4);RuntimeSpectralVolume_Free(v);
    }
    /* Enough coincident events to overflow the optional achromatic cache; multiword bands. */
    {
        RuntimeSpectralVolume *v=kernel_volume(.1);bind(v,&empty);int n=300;v->count=v->capacity=n;
        v->points=calloc(n,sizeof(*v->points));v->positions=calloc(n,sizeof(*v->positions));v->heads=calloc(65536,sizeof(int));assert(v->points&&v->positions&&v->heads);
        uint64_t cell=(UINT64_C(5)<<24)|(UINT64_C(5)<<12)|5;
        for(int i=0;i<n;i++) {
            int band=i%3==0?0:i%3==1?40:100;unsigned bucket=spectral_volume_bucket(cell,band);
            v->positions[i]=vec3(.5,.5,.5);v->points[i]=(SpectralCollision){.incoming={0,0,1},.watts=1./n,.cell=cell,.band=band,.next=v->heads[bucket],.first=true};v->heads[bucket]=i+1;
        }
        begin(v);
        for(int j=0;j<3;j++) {
            int band=j==0?0:j==1?40:100;double first=0;
            double a=spectral_reconstruction_source(v,vec3(.5,.5,.5),band,&first);assert(fabs(a-1/(12*M_PI*.001))<1e-10);assert(first==a);
            v->config.disable_camera_cache=true;first=0;double b=spectral_reconstruction_source(v,vec3(.5,.5,.5),band,&first);assert(a==b);v->config.disable_camera_cache=false;
        }
        RuntimeSpectralVolume_Free(v);
    }
    {
        RuntimeSpectralVolume *v=kernel_volume(.1);v->config.g=.9;bind(v,&empty);event(v,vec3(.5,.5,.5));begin(v);double a=sample(v,vec3(.5,.5,.5));
        v->reconstruction->generation=UINT64_MAX;assert(spectral_reconstruction_begin_ray(v,RuntimeRay3D_Make(vec3(.5,.5,-3),vec3(0,0,1))));
        double b=sample(v,vec3(.5,.5,.5));assert(a>b*1000);RuntimeSpectralVolume_Free(v);
    }
    {
        RuntimeSpectralVolume *v=kernel_volume(.2);v->config.local_refinement_step_m=.025;v->config.local_refinement_lo=vec3(.4,0,0);v->config.local_refinement_hi=vec3(.6,1,1);bind(v,&empty);
        SpectralFieldRay it;assert(spectral_field_begin(&it,v,RuntimeRay3D_Make(vec3(-.1,.5,.5),vec3(1,0,0)),2,true));
        double a,b,sum=0;int count=0,coarse=0,fine=0;
        while(spectral_field_next(&it,&a,&b)) { assert(++count<200);sum+=b-a;double x=(a+b)*.5-.1;if(x>.4&&x<.6){assert(b-a<=.025+1e-12);fine++;}else if(b-a>.1)coarse++; }
        assert(fabs(sum-1)<1e-12 && fine>=8 && coarse>0);RuntimeSpectralVolume_Free(v);
    }
    { RuntimeSpectralVolume *v=kernel_volume(.1);bind(v,&empty);begin(v);assert(sample(v,vec3(.5,.5,.5))==0);RuntimeSpectralVolume_Free(v); }
    { RuntimeSpectralVolume *v=kernel_volume(.1);bind(v,&empty);event(v,vec3(.5,.5,.5));v->reconstruction->budget_bytes=v->reconstruction->owned_bytes;assert(!RuntimeSpectralVolume_PrepareReconstruction(v));assert(strstr(RuntimeSpectralVolume_Error(v),"budget"));RuntimeSpectralVolume_Free(v); }
    puts("PASS analytic mesh kernel: mass/leakage, support classes, cache overflow/bands, phase wrap, local partitions, empty/budget");
}
