#pragma once
#include <math.h>
#include <assert.h>
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_native_3d_render_unit.h"

static void test_runtime_camera_pixel_sampling(void) {
    RuntimeNative3DSamplingContext s = {.sampleSequence=37, .cameraSequenceSeed=37,
        .cameraPixelSampler=RUNTIME_CAMERA_PIXEL_PROGRESSIVE, .temporalSubpassCount=16};
    RuntimeCamera3D camera = {0};camera.zoom=1;camera.nearPlane=.1;
    RuntimeCameraProjector3D projector;
    assert_true("camera_area_projector",RuntimeCameraProjector3D_Build(&camera,64,48,&projector));
    for(int mode=RUNTIME_CAMERA_PIXEL_STRATIFIED;mode<=RUNTIME_CAMERA_PIXEL_PROGRESSIVE;++mode) {
        s.cameraPixelSampler=mode;
        for(int pixel=0;pixel<32;++pixel) {
            int bins[16]={0};
            for(int i=0;i<16;++i) {
                s.temporalSubpassIndex=i;s.sampleSequence=37+i;
                double x,y,x2,y2;RuntimeNative3DSampling_PixelPosition(&s,pixel,7,&x,&y);
                double u=x-pixel+.5,v=y-7+.5;
                assert_true("camera_area_in_pixel",u>0 && u<1 && v>0 && v<1);
                bins[(int)(v*4)*4+(int)(u*4)]++;
                RuntimeNative3DSampling_PixelPosition(&s,pixel,7,&x2,&y2);
                assert_true("camera_area_repeat_exact",x==x2 && y==y2);
                Ray3D ray=RuntimeCameraProjector3D_MakePrimaryRay(&projector,x,y);
                Ray3D dx=RuntimeCameraProjector3D_MakePrimaryRay(&projector,x+1,y);
                Ray3D dy=RuntimeCameraProjector3D_MakePrimaryRay(&projector,x,y+1);
                assert_true("camera_area_dx_consistent",vec3_length(vec3_sub(ray.directionDx,dx.direction))<1e-12);
                assert_true("camera_area_dy_consistent",vec3_length(vec3_sub(ray.directionDy,dy.direction))<1e-12);
                if(mode==RUNTIME_CAMERA_PIXEL_PROGRESSIVE) {
                    s.temporalSubpassCount=64;s.sampleSequence+=1234;
                    RuntimeNative3DSampling_PixelPosition(&s,pixel,7,&x2,&y2);
                    assert_true("camera_area_budget_prefix_and_transport_independent",x==x2 && y==y2);
                    s.temporalSubpassCount=16;
                }
            }
            for(int i=0;i<16;++i)assert_true("camera_area_complete_4x4_coverage",bins[i]==1);
        }
    }
    s.cameraPixelSampler=RUNTIME_CAMERA_PIXEL_CENTER;
    double x,y;RuntimeNative3DSampling_PixelPosition(&s,3,5,&x,&y);
    assert_true("camera_area_center_compatibility",x==3 && y==5);
    s.cameraPixelSampler=RUNTIME_CAMERA_PIXEL_PROGRESSIVE;s.temporalSubpassCount=1;
    RuntimeNative3DSampling_PixelPosition(&s,3,5,&x,&y);
    assert_true("camera_area_single_pass_compatibility",x==3 && y==5);
    /* Independent area oracle: the triangle u+v<1 covers half the pixel.
     * Center-only sees exactly the same edge every time. */
    s.temporalSubpassCount=256;double error=0;
    for(int pixel=0;pixel<64;++pixel) {
        int hits=0;
        for(int i=0;i<256;++i) {
            s.temporalSubpassIndex=i;
            RuntimeNative3DSampling_PixelPosition(&s,pixel,5,&x,&y);
            if((x-pixel+.5)+(y-5+.5)<1)++hits;
        }
        error+=fabs(hits/256.0-.5);
    }
    assert_true("camera_area_diagonal_coverage_converges",error/64<.025);
    s.temporalSubpassCount=16;s.temporalSubpassIndex=4;
    s.cameraSequenceSeed=1;RuntimeNative3DSampling_PixelPosition(&s,3,5,&x,&y);
    double x2,y2;s.cameraSequenceSeed=2;RuntimeNative3DSampling_PixelPosition(&s,3,5,&x2,&y2);
    assert_true("camera_area_seed_changes_pattern",x!=x2 || y!=y2);
}

static void test_runtime_camera_pixel_sampling_native(void) {
    RuntimeNative3DPreparedFrame* frame=calloc(1,sizeof(*frame));assert(frame);
    RuntimeScene3D_Init(&frame->scene);
    frame->scene.environment.ambientIntensity=0;
    frame->scene.environment.backgroundIntensity=0;
    frame->scene.environment.topFillIntensity=0;
    RuntimeCamera3D camera={0};camera.zoom=1;camera.nearPlane=.1;
    assert_true("camera_area_native_projector",RuntimeCameraProjector3D_Build(&camera,32,24,&frame->projector));
    RuntimeLightSource3D source;RuntimeLightSource3D_Init(&source);
    source.kind=RUNTIME_LIGHT_SOURCE_3D_KIND_RECT;source.position=vec3(.13,-4,.07);
    source.axisU=vec3(cos(.37),0,sin(.37));source.axisV=vec3(-sin(.37),0,cos(.37));
    source.normal=vec3(0,1,0);source.width=1.2;source.height=.8;
    source.color=vec3(1,1,1);source.intensity=1;
    source.radiometryMode=RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE;source.radiance=1;
    source.emissionProfile=RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED;
    assert_true("camera_area_native_light",RuntimeLightSet3D_Append(&frame->scene.lightSet,&source,NULL));
    frame->valid=true;frame->width=32;frame->height=24;
    float full[32*24*RUNTIME_NATIVE_3D_RADIANCE_CHANNELS],part[16*12*RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    RuntimeNative3DRenderStats stats;
    frame->sampling=(RuntimeNative3DSamplingContext){.sampleSequence=37,.cameraSequenceSeed=37,
        .cameraPixelSampler=RUNTIME_CAMERA_PIXEL_PROGRESSIVE,.temporalSubpassCount=1024};
    double total=0;
    for(int sample=0;sample<1024;++sample) {
        frame->sampling.temporalSubpassIndex=sample;
        bool ok=RuntimeNative3DRenderPreparedRegionRadianceRGB(full,32,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
            frame,0,0,32,24,&stats);
        assert_true("camera_area_native_render",ok);
        for(int pixel=0;pixel<32*24;++pixel)total+=full[pixel*RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
        if(sample<4) {
            for(int y=0;y<24;y+=12)for(int x=0;x<32;x+=16) {
                assert_true("camera_area_native_partition",RuntimeNative3DRenderPreparedRegionRadianceRGB(
                    part,16,RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,frame,x,y,x+16,y+12,&stats));
                for(int py=0;py<12;++py)for(int px=0;px<16;++px)
                    for(int c=0;c<4;++c)assert_true("camera_area_native_partition_exact",
                        part[(py*16+px)*4+c]==full[((py+y)*32+px+x)*4+c]);
            }
        }
    }
    const double expected=source.width*source.height*32*24/
        (4*16*frame->projector.tanHalfFovX*frame->projector.tanHalfFovY);
    const double measured=total/1024;
    printf("CAMERA_AREA_ENERGY expected=%.12g measured=%.12g relative_error=%.12g\n",
        expected,measured,fabs(measured-expected)/expected);
    assert_true("camera_area_native_linear_energy",fabs(measured-expected)/expected<.005);
    RuntimeNative3DRenderUnit unit;RuntimeNative3DRenderUnit_Init(&unit);
    RuntimeNative3DAdaptiveSampling_SetRuntimeOverride(true,true);
    assert_true("camera_area_unit_setup",RuntimeNative3DRenderUnit_Setup(&unit,
        RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,frame,0,0,32,24,&frame->sampling,16,false));
    assert_true("camera_area_center_guides_cannot_prune",!unit.useAdaptiveSampling &&
        RuntimeNative3DRenderUnit_ShouldRenderSubpass(&unit,15));
    RuntimeNative3DRenderUnit_Free(&unit);
    RuntimeNative3DAdaptiveSampling_SetRuntimeOverride(false,false);
    RuntimeNative3DPreparedFrame_Free(frame);free(frame);
}
