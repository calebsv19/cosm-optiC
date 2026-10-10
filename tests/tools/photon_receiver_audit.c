/* Independent quadrature and estimator-mean checks, no scene/render buffers. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_sample_support_3d.h"
#include "render/runtime_caustic_photon_receiver_bsdf_3d.h"
#include "render/runtime_caustic_photon_receiver_accounting_3d.h"
#include "render/runtime_disney_v2_transmitted_caustic_3d.h"
#include "render/runtime_native_3d_render_shading_internal.h"
#include "render/runtime_caustic_photon_direct_consumer_3d.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static int failures, checks;
static void check(const char* name, bool passed, double value, double expected) {
    checks++; if (!passed) failures++;
    printf("{\"check\":\"%s\",\"passed\":%s,\"value\":%.17g,\"expected\":%.17g}\n",
           name, passed ? "true" : "false", value, expected);
}
static double integrate(unsigned population, double radius) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapRecord3D record = {0};
    RuntimeCausticPhotonMapQuery3D q;
    RuntimeCausticPhotonMapQueryResult3D result;
    RuntimeCausticPhotonMap3D_Init(&map);
    if (!RuntimeCausticPhotonMap3D_Allocate(&map, population)) return NAN;
    record.normal = vec3(0,0,1); record.incidentDirection = vec3(0,0,-1);
    record.flux = vec3(1.0 / population, .5 / population, .25 / population);
    record.pathPdf = 1.0 / population; record.queryRadius = radius;
    record.sceneObjectIndex = 2; record.primitiveIndex = 3; record.triangleIndex = 4;
    /* Prepared bounded supports are identical here; population scales packet
     * power, not a second division during reconstruction. */
    record.sampleCenteredSupportPrepared = true;
    record.sampleCenteredSupportRadius = radius;
    record.sampleCenteredSupportNeighborCount = population - 1;
    for (unsigned i = 0; i < population; i++) {
        record.photonId = i + 1;
        if (!RuntimeCausticPhotonMap3D_StoreRecord(&map, &record)) return NAN;
    }
    RuntimeCausticPhotonMap3D_DefaultQuery(&q);
    q.normal = record.normal; q.radius = radius;
    q.sceneObjectIndex=2; q.primitiveIndex=3; q.triangleIndex=4;
    q.estimator.estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST;
    q.estimator.minimumEffectiveSamples = 4;
    double power = 0;
    const int steps = 2000;
    for (int i = 0; i < steps; i++) {
        double distance = radius * (i + .5) / steps;
        q.position = vec3(distance,0,0);
        if (!RuntimeCausticPhotonMap3D_Query(&map,&q,&result)) return NAN;
        power += result.physicalFlux.x * 2 * M_PI * distance * radius / steps;
    }
    RuntimeCausticPhotonMap3D_Free(&map);
    return power;
}
static void* record_thread(void* unused) {
    (void)unused;
    RuntimeCausticPhotonMapQueryResult3D query={0};
    query.hit=true;query.effectiveSampleCount=4;query.supportRadius=.1;
    query.physicalFlux=vec3(1,1,1);
    for(int i=0;i<2000;i++)RuntimePhotonReceiverAccounting3D_Record(2,3,&query,vec3(.25,.25,.25));
    return NULL;
}
int main(void) {
    for (int r=0;r<3;r++) for (int p=0;p<3;p++) {
        const double radii[] = {.01,.045,.1}; const unsigned populations[] = {1,4,64};
        double value=integrate(populations[p],radii[r]);
        char name[100]; snprintf(name,sizeof(name),"normalized power radius %.3g population %u",radii[r],populations[p]);
        check(name,fabs(value-1)<1e-6,value,1);
    }
    RuntimeMaterialPayload3D material={0}; material.valid=true;
    material.baseColorR=.6; material.baseColorG=.4; material.baseColorB=.2;
    material.bsdf.diffuseWeight=.5;
    RuntimeCausticPhotonMapQueryResult3D query={0}; query.hit=true;
    query.physicalFlux=vec3(2,3,4); query.meanIncidentCosine=.3;
    Vec3 radiance;
    bool applied=RuntimeCausticPhotonReceiverBsdf3D_EvaluateDiffuse(&material,&query,&radiance,NULL);
    check("receiver Lambertian normalization once",applied && fabs(radiance.x-.6/M_PI)<1e-12,radiance.x,.6/M_PI);
    RuntimeDisneyV2_3DResult result={0};
    RuntimeDisneyV2TransmittedCaustic3D_AccumulateResolvedSample(&result,true,vec3(2,4,6),2.25,1.5,.5);
    RuntimeDisneyV2TransmittedCaustic3D_AccumulateResolvedSample(&result,false,vec3(0,0,0),1,1,1);
    RuntimeDisneyV2TransmittedCaustic3D_Finalize(&result,2);
    check("camera radiance weight greater than one retained",fabs(result.primaryTransmissionCausticRadianceR-2.25)<1e-12,result.primaryTransmissionCausticRadianceR,2.25);
    check("misses stay in camera estimator denominator",fabs(result.primaryTransmissionCausticRadianceG-3)<1e-12,result.primaryTransmissionCausticRadianceG,3);
    /* Independent estimator oracle: one of k equally weighted strata selects
     * the contributing branch. Its 1/p compensation must average to unit
     * radiance, including the other k-1 zero-contribution branches. */
    for (int period=2;period<=8;period*=2) {
        RuntimeDisneyV2_3DResult mean={0};
        for (int i=0;i<1024;i++)
            RuntimeDisneyV2TransmittedCaustic3D_AccumulateResolvedSample(&mean,i%period==0,vec3(1,1,1),period,period,period);
        RuntimeDisneyV2TransmittedCaustic3D_Finalize(&mean,1024);
        char name[100];snprintf(name,sizeof(name),"compensated caustic branch mean p=1/%d",period);
        check(name,fabs(mean.primaryTransmissionCausticRadianceR-1)<1e-12,mean.primaryTransmissionCausticRadianceR,1);
    }
    memset(&result,0,sizeof(result));
    RuntimeDisneyV2TransmittedCaustic3D_AccumulateResolvedSample(&result,true,vec3(1,1,1),NAN,INFINITY,-1);
    check("invalid camera weights rejected",result.primaryTransmissionCausticContributingSampleCount==0,result.primaryTransmissionCausticContributingSampleCount,0);
    RuntimeCausticPhotonMap3D map; RuntimeCausticPhotonMap3D_Init(&map);
    RuntimeCausticPhotonMap3D_Allocate(&map,4);
    RuntimeCausticPhotonMapRecord3D record={0};
    record.normal=vec3(0,0,1); record.incidentDirection=vec3(0,0,-1);
    record.flux=vec3(.001,.001,.001);record.queryRadius=.1;record.pathPdf=1;
    record.sceneObjectIndex=2;record.primitiveIndex=3;record.triangleIndex=4;
    for(int i=0;i<4;i++) { record.photonId=i+1;RuntimeCausticPhotonMap3D_StoreRecord(&map,&record); }
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    RuntimeCausticPhotonDirectConsumer3D_Bind(RUNTIME_CAUSTIC_PHOTON_CONSUMER_DIRECT_MAP,&map,NULL,&settings);
    memset(&result,0,sizeof(result));
    result.primaryTransmissionContinued=true;result.primaryTransmissionPathState.valid=true;
    result.primaryTransmissionPathState.hit=true;
    HitInfo3D_Reset(&result.primaryTransmissionPathState.hitInfo);
    result.primaryTransmissionPathState.hitInfo.position=vec3(0,0,0);
    result.primaryTransmissionPathState.hitInfo.normal=record.normal;
    result.primaryTransmissionPathState.hitInfo.sceneObjectIndex=2;
    result.primaryTransmissionPathState.hitInfo.primitiveIndex=3;
    result.primaryTransmissionPathState.hitInfo.triangleIndex=4;
    result.primaryTransmissionCameraThroughputR=2.25;
    result.primaryTransmissionCameraThroughputG=2.25;
    result.primaryTransmissionCameraThroughputB=2.25;
    Vec3 receiver;
    RuntimeCausticPhotonDirectConsumer3D_SampleSurface(&result.primaryTransmissionPathState.hitInfo,&receiver,NULL);
    RuntimeCausticSurfaceCache3D cache={0};RuntimeNative3DRenderStats stats={0};
    double red=0,green=0,blue=0,luma=0;bool visible=false;
    runtime_native_3d_render_apply_transmitted_surface_caustic_cache(&cache,&result,&red,&green,&blue,&luma,&visible,&stats);
    check("fallback camera radiance weight retained",visible && receiver.x>0 && fabs(red-2.25*receiver.x)<1e-12,red,2.25*receiver.x);
    result.primaryTransmissionCausticDirectMapSampled=true;
    result.primaryTransmissionCausticRadianceR=2.25*receiver.x;
    red=green=blue=luma=0;
    runtime_native_3d_render_apply_transmitted_surface_caustic_cache(&cache,&result,&red,&green,&blue,&luma,&visible,&stats);
    check("resolved caustic is weighted exactly once",fabs(red-2.25*receiver.x)<1e-12,red,2.25*receiver.x);
    RuntimeCausticPhotonDirectConsumer3D_Reset();RuntimeCausticPhotonMap3D_Free(&map);
    RuntimePhotonReceiverAccounting3D a;
    RuntimePhotonReceiverAccounting3D_SetEnabled(false);RuntimePhotonReceiverAccounting3D_Reset();
    RuntimePhotonReceiverAccounting3D_Record(1,2,&query,radiance);
    RuntimePhotonReceiverAccounting3D_Snapshot(&a);
    check("disabled accounting has zero records",a.bucketCount==0,a.bucketCount,0);
    RuntimePhotonReceiverAccounting3D_SetEnabled(true);
    query.undersampled=true;query.effectiveSampleCount=1;query.supportRadius=.1;
    for(int i=0;i<70;i++)RuntimePhotonReceiverAccounting3D_Record(i,2,&query,radiance);
    RuntimePhotonReceiverAccounting3D_Snapshot(&a);
    check("receiver capacity explicit overflow",a.bucketCount==64 && a.overflowQueryCount==6,a.overflowQueryCount,6);
    check("receiver input and output reconcile",a.buckets[0].queryCount==1 && a.buckets[0].positiveQueryCount==1 && a.buckets[0].undersampledPositiveQueryCount==1 && fabs(a.buckets[0].radianceSum.x-radiance.x)<1e-12,a.buckets[0].radianceSum.x,radiance.x);
    RuntimePhotonReceiverAccounting3D_Reset();RuntimePhotonReceiverAccounting3D_Snapshot(&a);
    check("reset prevents previous-frame accounting",a.bucketCount==0 && a.overflowQueryCount==0,a.bucketCount,0);
    pthread_t workers[4];
    for(int i=0;i<4;i++)pthread_create(&workers[i],NULL,record_thread,NULL);
    for(int i=0;i<4;i++)pthread_join(workers[i],NULL);
    RuntimePhotonReceiverAccounting3D_Snapshot(&a);
    check("concurrent receiver calls reconcile",a.bucketCount==1 && a.buckets[0].queryCount==8000 && a.buckets[0].radianceSum.x==2000,a.buckets[0].queryCount,8000);
    fprintf(stderr,"%d checks, %d failures; %zu fixed accounting bytes\n",checks,failures,sizeof(a));
    return failures ? 1 : 0;
}
