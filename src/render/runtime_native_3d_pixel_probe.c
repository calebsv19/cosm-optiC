#include "render/runtime_native_3d_pixel_probe.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t output_lock = PTHREAD_MUTEX_INITIALIZER;
static int coordinates[16][2];
static int coordinate_count;
static bool selected_only;

static void initialize(void) {
    const char* value = getenv("RAY_TRACING_NATIVE_3D_PIXEL_PROBE");
    if (!value || !*value) return;
    const char* cursor = value;
    while (*cursor && coordinate_count < 16) {
        char* end = NULL;
        errno = 0;
        long x = strtol(cursor,&end,10);
        if (end == cursor || errno || *end != ',' || x < 0 || x > INT_MAX) {
            coordinate_count = 0; return;
        }
        cursor = end + 1;
        errno = 0;
        long y = strtol(cursor,&end,10);
        if (end == cursor || errno || y < 0 || y > INT_MAX) {
            coordinate_count = 0; return;
        }
        coordinates[coordinate_count][0] = (int)x;
        coordinates[coordinate_count++][1] = (int)y;
        cursor = end;
        if (!*cursor) {
            const char* only = getenv("RAY_TRACING_NATIVE_3D_PIXEL_PROBE_ONLY");
            selected_only = only && only[0] == '1' && only[1] == '\0';
            return;
        }
        if (*cursor++ != ';' || !*cursor) { coordinate_count = 0; return; }
    }
    if (*cursor) coordinate_count = 0;
}

static int selected(int x, int y) {
    pthread_once(&init_once, initialize);
    for (int i = 0; i < coordinate_count; ++i)
        if (coordinates[i][0] == x && coordinates[i][1] == y) return 1;
    return 0;
}

bool RuntimeNative3DPixelProbe_IsSelected(int x, int y) { return selected(x,y); }

void RuntimeNative3DPixelProbe_Receiver(int x, int y, const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload, const Ray3D* ray) {
    if (!hit || !payload || !ray || !selected(x,y)) return;
    pthread_mutex_lock(&output_lock);
    fprintf(stderr,"PIXEL_PROBE {\"stage\":\"receiver\",\"x\":%d,\"y\":%d,"
        "\"albedo\":[%.9g,%.9g,%.9g],\"position\":[%.9g,%.9g,%.9g],"
        "\"footprint\":%d,\"unbounded\":%d,\"transported\":%d,"
        "\"dx\":[%.9g,%.9g,%.9g],\"dy\":[%.9g,%.9g,%.9g],"
        "\"direction\":[%.9g,%.9g,%.9g]}\n",x,y,
        payload->baseColorR,payload->baseColorG,payload->baseColorB,
        hit->position.x,hit->position.y,hit->position.z,
        hit->hasPixelFootprint,hit->footprintUnbounded,hit->footprintTransported,
        hit->pixelDpDx.x,hit->pixelDpDx.y,hit->pixelDpDx.z,
        hit->pixelDpDy.x,hit->pixelDpDy.y,hit->pixelDpDy.z,
        ray->direction.x,ray->direction.y,ray->direction.z);
    pthread_mutex_unlock(&output_lock);
}

bool RuntimeNative3DPixelProbe_ShouldShade(int x, int y) {
    pthread_once(&init_once, initialize);
    return !selected_only || selected(x,y);
}

void RuntimeNative3DPixelProbe_Shade(int x, int y,
    const RuntimeNative3DSamplingContext* sampling,
    const RuntimeDisneyV2_3DResult* r, double ar, double ag, double ab) {
    if (!r || !selected(x, y)) return;
    pthread_mutex_lock(&output_lock);
    fprintf(stderr, "PIXEL_PROBE {\"stage\":\"shade\",\"x\":%d,\"y\":%d,"
        "\"sequence\":%u,\"subpass\":%u,\"object\":%d,\"triangle\":%d,"
        "\"normal\":[%.9g,%.9g,%.9g],\"albedo\":[%.9g,%.9g,%.9g],"
        "\"footprint\":%d,\"unbounded\":%d,\"dx\":[%.9g,%.9g,%.9g],\"dy\":[%.9g,%.9g,%.9g],"
        "\"roughness\":%.9g,\"rgb\":[%.9g,%.9g,%.9g],"
        "\"diffuse\":[%.9g,%.9g,%.9g],\"specular\":[%.9g,%.9g,%.9g],"
        "\"stochastic_direct\":[%.9g,%.9g,%.9g],\"stochastic_bsdf\":[%.9g,%.9g,%.9g],"
        "\"recursive_direct\":[%.9g,%.9g,%.9g],\"recursive_bsdf\":[%.9g,%.9g,%.9g],"
        "\"transmission\":[%.9g,%.9g,%.9g],\"primary_transmission\":[%.9g,%.9g,%.9g],"
        "\"ambient\":[%.9g,%.9g,%.9g],\"light_pdf\":%.9g,\"bsdf_pdf\":%.9g,"
        "\"light_mis_weight\":%.9g,\"bsdf_mis_weight\":%.9g,"
        "\"sampled_lobe\":%d,\"path_pdf\":%.9g,\"path_throughput\":[%.9g,%.9g,%.9g],"
        "\"path_emitter_wins\":%d}\n",
        x,y,sampling?sampling->sampleSequence:0,sampling?sampling->temporalSubpassIndex:0,
        r->hitInfo.sceneObjectIndex,r->hitInfo.triangleIndex,
        r->hitInfo.normal.x,r->hitInfo.normal.y,r->hitInfo.normal.z,
        r->principled.baseColorR,r->principled.baseColorG,r->principled.baseColorB,
        r->hitInfo.hasPixelFootprint,r->hitInfo.footprintUnbounded,
        r->hitInfo.pixelDpDx.x,r->hitInfo.pixelDpDx.y,r->hitInfo.pixelDpDx.z,
        r->hitInfo.pixelDpDy.x,r->hitInfo.pixelDpDy.y,r->hitInfo.pixelDpDy.z,r->principled.roughness,
        r->radianceR,r->radianceG,r->radianceB,
        r->diffuseRadianceR,r->diffuseRadianceG,r->diffuseRadianceB,
        r->specularRadianceR,r->specularRadianceG,r->specularRadianceB,
        r->stochasticDirectRadianceR,r->stochasticDirectRadianceG,r->stochasticDirectRadianceB,
        r->stochasticBsdfRadianceR,r->stochasticBsdfRadianceG,r->stochasticBsdfRadianceB,
        r->recursiveDirectRadianceR,r->recursiveDirectRadianceG,r->recursiveDirectRadianceB,
        r->recursiveBsdfRadianceR,r->recursiveBsdfRadianceG,r->recursiveBsdfRadianceB,
        r->transmissionRadianceR,r->transmissionRadianceG,r->transmissionRadianceB,
        r->primaryTransmissionRadianceR,r->primaryTransmissionRadianceG,r->primaryTransmissionRadianceB,
        ar,ag,ab,r->lightSamplePdf,r->bsdfSamplePdf,r->misWeightLight,r->misWeightBsdf,
        r->sampledLobe,r->pathState.pdf,r->pathState.throughputR,
        r->pathState.throughputG,r->pathState.throughputB,r->pathState.emitterWins);
    for (int v=0; v<r->misVertexCount && v<RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY; ++v) {
        fprintf(stderr,"PIXEL_PROBE {\"stage\":\"mis_vertex\",\"x\":%d,\"y\":%d,\"subpass\":%u,"
            "\"vertex\":%d,\"light_pdf\":%.9g,\"bsdf_pdf\":%.9g,\"light_weight\":%.9g,\"bsdf_weight\":%.9g,"
            "\"light_contribution\":[%.9g,%.9g,%.9g],\"bsdf_contribution\":[%.9g,%.9g,%.9g]}\n",
            x,y,sampling?sampling->temporalSubpassIndex:0,v,
            r->misVertexLightPdf[v],r->misVertexBsdfPdf[v],r->misVertexWeightLight[v],r->misVertexWeightBsdf[v],
            r->lightSampleContributionR[v],r->lightSampleContributionG[v],r->lightSampleContributionB[v],
            r->bsdfSampleContributionR[v],r->bsdfSampleContributionG[v],r->bsdfSampleContributionB[v]);
    }
    for (int v=0; v<r->recursiveLoopVertexCount && v<RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY; ++v) {
        const RuntimeDisneyV2_3DPathState* state=&r->recursiveLoopStates[v];
        fprintf(stderr,"PIXEL_PROBE {\"stage\":\"recursive_state\",\"x\":%d,\"y\":%d,\"subpass\":%u,"
            "\"slot\":%d,\"depth\":%d,\"next_hit_object\":%d,\"next_hit_triangle\":%d,\"source_roughness\":%.9g,"
            "\"pdf\":%.9g,\"throughput\":[%.9g,%.9g,%.9g],\"emitter_wins\":%d,\"termination\":%d}\n",
            x,y,sampling?sampling->temporalSubpassIndex:0,v,state->depth,
            state->hit?state->hitInfo.sceneObjectIndex:-1,state->hit?state->hitInfo.triangleIndex:-1,
            r->recursiveLoopPrincipled[v].roughness,state->pdf,
            state->throughputR,state->throughputG,state->throughputB,state->emitterWins,
            r->recursiveLoopTerminationReasons[v]);
    }
    pthread_mutex_unlock(&output_lock);
}

void RuntimeNative3DPixelProbe_Denoise(int x, int y,
    const RuntimeNative3DFeatureBuffer* f, size_t pixel,
    const float* before, const float* after, float blend,
    int neighbors, float weight, const char* decision) {
    if (!f || !before || !after || !selected(x,y)) return;
    size_t local = pixel;
    const RuntimeNative3DFeatureBuffer* guide =
        RuntimeNative3DFeatureBuffer_GuideSource(f,pixel,&local);
    const float variance = RuntimeNative3DFeatureBuffer_VarianceAt(guide,local);
    char variance_text[48];
    if (isfinite(variance)) snprintf(variance_text,sizeof(variance_text),"%.9g",variance);
    else snprintf(variance_text,sizeof(variance_text),"null");
    pthread_mutex_lock(&output_lock);
    fprintf(stderr,"PIXEL_PROBE {\"stage\":\"denoise\",\"x\":%d,\"y\":%d,"
        "\"decision\":\"%s\",\"object\":%d,\"roughness\":%.9g,\"transparency\":%.9g,"
        "\"samples\":%u,\"variance\":%s,\"blend\":%.9g,\"neighbors\":%d,\"weight\":%.9g,"
        "\"before\":[%.9g,%.9g,%.9g],\"after\":[%.9g,%.9g,%.9g]}\n",
        x,y,decision,f->sceneObjectIndexBuffer[pixel],f->roughnessBuffer[pixel],
        f->transparencyBuffer[pixel],guide&&guide->sampleCountBuffer?guide->sampleCountBuffer[local]:0,
        variance_text,blend,neighbors,weight,
        before[0],before[1],before[2],after[0],after[1],after[2]);
    pthread_mutex_unlock(&output_lock);
}
