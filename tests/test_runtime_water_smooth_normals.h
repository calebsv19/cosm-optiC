#pragma once
#include "render/runtime_ray_3d.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_heightfield_normals_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

static void test_runtime_water_smooth_normals(void) {
    float h[25];
    for(int y=0;y<5;++y)for(int x=0;x<5;++x) {
        double xx=-1+.5*x, yy=-1+.5*y;h[y*5+x]=(float)(1+.08*xx*xx+.05*yy*yy);
    }
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);
    RuntimeWaterBodyMesh3DDesc d={.object_id="smooth_water",.material_id="water",
        .medium_id="water",.scene_object_index=0,.grid_w=5,.grid_d=5,.heights_y=h,
        .sample_origin_x=-1,.sample_origin_z=-1,.sample_spacing_x=.5,.sample_spacing_z=.5,
        .bottom_height=0,.map_y_height_to_scene_z=true};
    RuntimeWaterBodyMesh3DReport report;
    assert_true("smooth_water_mesh",RuntimeWaterBodyMesh3D_Append(&scene,&d,&report));
    assert_true("smooth_water_closed_topology",report.topology_valid && report.boundary_edge_count==0);
    for(int i=0;i<report.top_triangle_count;++i) {
        RuntimeTriangle3D *t=&scene.triangleMesh.triangles[i];
        assert_true("smooth_water_top_vertex_normals",t->hasVertexNormals);
        Vec3 points[]={t->p0,t->p1,t->p2};Vec3 ns[]={t->vertexNormal0,t->vertexNormal1,t->vertexNormal2};
        for(int j=0;j<3;++j) {
            assert_close("smooth_water_unit",vec3_length(ns[j]),1,1e-12);
            assert_true("smooth_water_geometric_hemisphere",vec3_dot(ns[j],t->normal)>0);
            if(fabs(points[j].x)<.99 && fabs(points[j].y)<.99) {
                Vec3 expected=vec3_normalize(vec3(-.16*points[j].x,-.10*points[j].y,1));
                assert_close("smooth_water_curve_gradient",vec3_length(vec3_sub(ns[j],expected)),0,2e-7);
            }
        }
    }
    for(int i=0;i<report.top_triangle_count;++i)for(int j=i+1;j<report.top_triangle_count;++j) {
        RuntimeTriangle3D *a=&scene.triangleMesh.triangles[i],*b=&scene.triangleMesh.triangles[j];
        Vec3 ap[]={a->p0,a->p1,a->p2},bp[]={b->p0,b->p1,b->p2};
        Vec3 an[]={a->vertexNormal0,a->vertexNormal1,a->vertexNormal2},bn[]={b->vertexNormal0,b->vertexNormal1,b->vertexNormal2};
        for(int u=0;u<3;++u)for(int v=0;v<3;++v)
            if(vec3_length(vec3_sub(ap[u],bp[v]))<1e-12)
                assert_close("smooth_water_adjacent_shared_vertex",vec3_length(vec3_sub(an[u],bn[v])),0,0);
    }
    for(int i=report.top_triangle_count;i<scene.triangleMesh.triangleCount;++i)
        assert_true("smooth_water_wall_bottom_crease",!scene.triangleMesh.triangles[i].hasVertexNormals);
    RuntimeTriangle3D *a=&scene.triangleMesh.triangles[0],*b=&scene.triangleMesh.triangles[1];
    assert_close("smooth_water_shared_diagonal_normal0",vec3_length(vec3_sub(a->vertexNormal0,b->vertexNormal0)),0,0);
    assert_close("smooth_water_shared_diagonal_normal1",vec3_length(vec3_sub(a->vertexNormal1,b->vertexNormal2)),0,0);
    assert_true("smooth_water_bvh",RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh));
    RuntimeRay3DTraceRoute route=RuntimeRay3D_CurrentTraceRoute();
    RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    RuntimeCamera3D camera={0};camera.position=vec3(0,-2,3);camera.lookPitch=-.9;camera.rotation=3.141592653589793;camera.zoom=1;
    RuntimeCameraProjector3D projector;RuntimeCameraProjector3D_Build(&camera,256,256,&projector);
    Ray3D ray=RuntimeCameraProjector3D_MakePrimaryRay(&projector,128,128);HitInfo3D smooth,faceted;
    assert_true("smooth_water_actual_hit",RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,.01,100,&smooth));
    assert_true("smooth_water_normal_field",!smooth.constantShadingNormal && smooth.hasReflectionNormalField);
    Vec3 normals[2];
    assert_true("smooth_water_neighbor_field",RuntimeRay3D_NormalFootprintNeighbors(&smooth,smooth.normal,normals));
    assert_true("smooth_water_neighbor_changes",vec3_length(vec3_sub(normals[0],smooth.normal))>1e-5);
    for(int i=0;i<report.top_triangle_count;++i)scene.triangleMesh.triangles[i].hasVertexNormals=false;
    assert_true("smooth_water_faceted_hit",RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,.01,100,&faceted));
    assert_close("smooth_water_position_preserved",vec3_length(vec3_sub(smooth.position,faceted.position)),0,0);
    assert_close("smooth_water_geometric_normal_preserved",vec3_length(vec3_sub(smooth.geometricNormal,faceted.geometricNormal)),0,0);
    assert_true("smooth_water_different_shading_normal",vec3_length(vec3_sub(smooth.shadingNormal,faceted.shadingNormal))>1e-3);
    RuntimeRay3D_SetTraceRouteForTests(route);RuntimeScene3D_Free(&scene);
}
