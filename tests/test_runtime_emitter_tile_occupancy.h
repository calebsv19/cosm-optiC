#pragma once
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_light_set_3d.h"

static void test_runtime_emitter_tile_occupancy(void) {
    RuntimeScene3D scene; RuntimeScene3D_Init(&scene);
    RuntimeCamera3D camera = {0}; camera.position=vec3(0,0,0); camera.zoom=1; camera.nearPlane=.1;
    RuntimeCameraProjector3D projector;
    assert_true("emitter_occupancy_projector",RuntimeCameraProjector3D_Build(&camera,128,96,&projector));
    RuntimeLightSource3D source; RuntimeLightSource3D_Init(&source);
    source.kind=RUNTIME_LIGHT_SOURCE_3D_KIND_RECT;source.position=vec3(.9,-4,1.1);
    source.axisU=vec3(1,0,0);source.axisV=vec3(0,0,1);source.normal=vec3(0,1,0);
    source.width=.7;source.height=.5;source.intensity=2;
    assert_true("emitter_occupancy_rect_append",RuntimeLightSet3D_Append(&scene.lightSet,&source,NULL));
    source.kind=RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE;source.radius=.15;source.position=vec3(-1,-4,-.6);
    assert_true("emitter_occupancy_second_append",RuntimeLightSet3D_Append(&scene.lightSet,&source,NULL));
    RuntimeNative3DTileOccupancy occupancy={0};
    assert_true("emitter_occupancy_build",RuntimeNative3DTileOccupancy_Build(&occupancy,&scene,&projector,16));
    int emitter_pixels=0;
    for(int y=0;y<96;++y)for(int x=0;x<128;++x) {
        for(int corner=0;corner<5;++corner) {
            double dx=corner==4?0:((corner&1)?.499:-.499);
            double dy=corner==4?0:((corner&2)?.499:-.499);
            Ray3D ray=RuntimeCameraProjector3D_MakePrimaryRay(&projector,x+dx,y+dy);
            RuntimeLightEmitterHit3DResult hit;
            if(RuntimeLightEmitter3D_IntersectRay(&scene,&ray,.1,100,&hit)) {
                ++emitter_pixels;
                assert_true("emitter_occupancy_no_subpixel_false_negatives",
                    RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&occupancy,x,y,x+1,y+1));
            }
        }
    }
    assert_true("emitter_occupancy_both_sources_visible",emitter_pixels>20);
    RuntimeNative3DTileOccupancy_Free(&occupancy);
    scene.lightSet.lights[0].enabled=false;scene.lightSet.lights[1].enabled=false;
    assert_true("emitter_occupancy_disabled_build",RuntimeNative3DTileOccupancy_Build(&occupancy,&scene,&projector,16));
    assert_true("emitter_occupancy_disabled_empty",!RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&occupancy,0,0,128,96));
    RuntimeNative3DTileOccupancy_Free(&occupancy);RuntimeScene3D_Free(&scene);
}
