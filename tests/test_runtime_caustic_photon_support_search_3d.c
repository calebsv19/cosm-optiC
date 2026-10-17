#include "render/runtime_caustic_photon_sample_support_3d.h"
#include "render/runtime_caustic_photon_surface_kernel_3d.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long support_checks;
static uint64_t random_bits(uint64_t* state) {
    *state ^= *state << 13; *state ^= *state >> 7; *state ^= *state << 17;
    return *state;
}
static double unit(uint64_t* state) {
    return (double)(random_bits(state) >> 11) / 9007199254740992.0;
}
static int compare_fixture(uint64_t seed, uint64_t n, uint64_t k, unsigned kind) {
    RuntimeCausticPhotonMapRecord3D* a = calloc((size_t)n, sizeof(*a));
    RuntimeCausticPhotonMapRecord3D* b = calloc((size_t)n, sizeof(*b));
    if (!a || !b) { free(a); free(b); return 1; }
    for (uint64_t i = 0; i < n; ++i) {
        a[i].position = vec3(unit(&seed)*2-1, unit(&seed)*2-1, unit(&seed)*2-1);
        a[i].normal = vec3(0,0,1);
        a[i].queryRadius = .05 + unit(&seed);
        a[i].sceneObjectIndex = (int)(i % 3);
        a[i].materialId = (int)(i % 5) - 1;
        a[i].flux = vec3(unit(&seed), unit(&seed), unit(&seed));
        if (kind == 1u) { /* dense ties; full neighbor count must survive heap truncation */
            a[i].position = vec3(0,0,0); a[i].materialId = -1; a[i].sceneObjectIndex=0;
        } else if (kind == 2u) { /* exact radius and neighboring floating values */
            double x = (double)(i % 7u) * .125;
            if (i % 3u == 1u) x = nextafter(x, -INFINITY);
            if (i % 3u == 2u) x = nextafter(x, INFINITY);
            a[i].position=vec3(x,0,0); a[i].queryRadius=.125;
            a[i].materialId=-1; a[i].sceneObjectIndex=0;
            a[i].normal = vec3(0,0,(i%4u==0u ? .25 : 1));
        } else if (kind == 3u) { /* split coordinates at large translated values */
            a[i].position.x += 1e14; a[i].position.y -= 1e14;
        } else if (kind == 4u) { /* unsupported domains must use exact fallback */
            if (i == 0u) a[i].position.x = NAN;
            if (i == 1u) a[i].queryRadius = INFINITY;
            if (i == 2u) a[i].queryRadius = 1e-200;
        } else if (kind >= 6u) { /* dense patches exercise whole-node counts */
            a[i].position=vec3(unit(&seed)*.01,unit(&seed)*.01,unit(&seed)*.001);
            a[i].queryRadius=.045;a[i].sceneObjectIndex=0;a[i].materialId=0;
            if(kind==7u) { /* wildcard/mixed receivers must not bulk accept */
                a[i].sceneObjectIndex=(int)(i%2);a[i].materialId=(int)(i%3)-1;
                a[i].normal=vec3(0,0,i%5==0 ? .125 : 1);
            } else if(kind==8u) { /* valid tiny scales and strict boundaries */
                a[i].position=vec3((double)(i%7)*1e-99,0,0);a[i].queryRadius=2e-99;
            } else if(kind==9u) { /* non-finite normal cannot qualify a node */
                a[i].normal=vec3(i%3==0 ? NAN : 0,0,i%2==0 ? .25 : 1);
            }
        } else if (kind == 5u) { /* sparse uniform field */
            a[i].position=vec3((double)(i%64u), (double)(i/64u), 0);
            a[i].queryRadius=.01;
        }
    }
    memcpy(b,a,(size_t)n*sizeof(*a));
    RuntimeCausticPhotonSampleSupportReadback3D ra, rb;
    bool oka=RuntimeCausticPhotonSampleSupport3D_Prepare(a,n,k,.25,&ra);
    bool okb=RuntimeCausticPhotonSampleSupport3D_PrepareBruteForce(b,n,k,.25,&rb);
    int failed = oka != okb || memcmp(a,b,(size_t)n*sizeof(*a)) != 0 ||
        ra.valid != rb.valid || ra.recordCount != rb.recordCount ||
        ra.neighborLimit != rb.neighborLimit || ra.adaptiveRecordCount != rb.adaptiveRecordCount ||
        ra.maximumRadiusRecordCount != rb.maximumRadiusRecordCount ||
        memcmp(&ra.minimumSupportRadius,&rb.minimumSupportRadius,sizeof(double)) ||
        memcmp(&ra.maximumSupportRadius,&rb.maximumSupportRadius,sizeof(double)) ||
        memcmp(&ra.meanSupportRadius,&rb.meanSupportRadius,sizeof(double));
    if (kind==4u && ra.spatialIndexUsed) failed=1;
    if (kind==6u && n>=4096u && ra.searchCandidateCount >= rb.searchCandidateCount/4u) failed=1;
    if (kind==5u && n>=4096u && ra.searchCandidateCount >= rb.searchCandidateCount/4u) failed=1;
    if (failed) fprintf(stderr,"FAIL photon support n=%llu k=%llu kind=%u\n",
                       (unsigned long long)n,(unsigned long long)k,kind);
    support_checks += n;
    free(a); free(b); return failed;
}
typedef struct {
    RuntimeCausticPhotonMapRecord3D records[257];
    RuntimeCausticPhotonSampleSupportReadback3D readback;
    bool ok;
} ThreadFixture;
static void* prepare_thread(void* data) {
    ThreadFixture* t=data;
    t->ok=RuntimeCausticPhotonSampleSupport3D_Prepare(t->records,257,16,.25,&t->readback);
    return NULL;
}
int run_test_runtime_caustic_photon_support_search_3d_tests(void) {
    int failures=0;
    uint64_t sizes[]={1,2,31,32,33,257,1001};
    uint64_t limits[]={1,4,64,2048};
    for (unsigned kind=0;kind<10;kind++)
        for (unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++)
            for (unsigned j=0;j<sizeof(limits)/sizeof(*limits);j++)
                failures+=compare_fixture(0xfedcab123ULL+kind+i,sizes[i],limits[j],kind);
    for (unsigned seed=1;seed<=200;seed++)
        failures+=compare_fixture(seed,33+seed*3u,1+seed%256u,seed%4u);
    failures+=compare_fixture(8,4096,64,5);
    failures+=compare_fixture(32,4096,64,6);
    failures+=compare_fixture(33,4096,64,7);
    ThreadFixture t[4]={0}; pthread_t threads[4];
    RuntimeCausticPhotonMapRecord3D expected[4][257];
    for (int i=0;i<4;i++) {
        for(int j=0;j<257;j++) { t[i].records[j].position=vec3(j*.001,i*.1,0);
            t[i].records[j].normal=vec3(0,0,1);t[i].records[j].queryRadius=.045; }
        memcpy(expected[i],t[i].records,sizeof(expected[i]));
        if(pthread_create(&threads[i],NULL,prepare_thread,&t[i])!=0) abort();
    }
    for(int i=0;i<4;i++) {
        pthread_join(threads[i],NULL);
        RuntimeCausticPhotonSampleSupportReadback3D r;
        failures+=!RuntimeCausticPhotonSampleSupport3D_PrepareBruteForce(expected[i],257,16,.25,&r);
        failures+=!t[i].ok || memcmp(expected[i],t[i].records,sizeof(expected[i]))!=0;
    }
    /* Independent radial area quadrature: normalized kernel integrates to one. */
    const double radii[]={.001,.045,.1,2};
    for(unsigned j=0;j<sizeof(radii)/sizeof(*radii);j++) {
        double integral=0, radius=radii[j], dr=radius/20000.0;
        for(int i=0;i<20000;i++) {double r=(i+.5)*dr;
            integral+=2*acos(-1)*r*RuntimeCausticPhotonSurfaceKernel3D_Weight(r,radius)*dr;}
        if(fabs(integral-1)>1e-8) {fprintf(stderr,"FAIL kernel energy %.17g\n",integral);failures++;}
    }
    printf("Photon support oracle: %llu record comparisons; normalized-kernel quadrature; concurrent calls; failures=%d\n",support_checks,failures);
    return failures;
}
#ifdef PHOTON_SUPPORT_TEST_MAIN
int main(void) {return run_test_runtime_caustic_photon_support_search_3d_tests()!=0;}
#endif
