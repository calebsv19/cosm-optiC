/* Camera-path sampling only. Forward photons and wavelength PDFs are unchanged. */
#include "runtime_spectral_surface_path_internal.h"
#include "render/runtime_native_3d_sampling.h"
#include <limits.h>

static uint64_t mix(uint64_t x) {
    x^=x>>30;x*=UINT64_C(0xbf58476d1ce4e5b9);
    x^=x>>27;x*=UINT64_C(0x94d049bb133111eb);return x^(x>>31);
}
/* Distinct prime bases for the first 16 draws at each of four transport events.
 * Prefix-dependent affine digit permutations are bijections in a prime base.
 * Scramble trailing zero digits as well; truncating at the input's last digit
 * would place sample zero at an endpoint and bias the randomized sequence.
 * Unlike reusing the first two Sobol matrices, these are distinct dimensions.
 * Later events/draws use independent hashed fallback, never dimension wrap. */
static double transport_sample(unsigned sample,unsigned dimension,uint64_t seed) {
    static const unsigned primes[64]={
        5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,
        67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,
        149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,
        233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317};
    unsigned base=primes[dimension];double value=0,place=1;
    uint64_t prefix=seed;
    do {
        unsigned digit=sample%base;sample/=base;
        uint64_t h=mix(prefix);
        unsigned a=1+(unsigned)(h%(base-1)),b=(unsigned)(mix(h)%base);
        place/=base;value+=((a*digit+b)%base)*place;
        prefix=mix(prefix^((uint64_t)digit+UINT64_C(0x9e3779b97f4a7c15)));
    } while(place>0x1p-54);
    return fmin(nextafter(1.,0.),fmax(0x1p-54,value));
}
PathRandom spectral_path_random_init(uint64_t pixel,unsigned seed,unsigned sample,bool qmc) {
    uint64_t stream=((uint64_t)seed<<32)^(pixel*UINT64_C(0x9e3779b97f4a7c15));
    return (PathRandom){.key=stream^((uint64_t)sample*UINT64_C(0x94d049bb133111eb)),
        .stream_key=stream,.sample_index=sample,.cached_pair=UINT_MAX,.qmc=qmc};
}
void spectral_path_random_event(PathRandom *r,unsigned event) {
    /* Primary pixel position has domain zero. Variable event draw counts cannot
     * shift dimensions at later events. Legacy sequencing is bit-for-bit fixed. */
    if(r->qmc) {r->event=event+1;r->dimension=0;r->cached_pair=UINT_MAX;}
}
void spectral_path_random_fork(PathRandom *r,uint64_t salt) {
    r->key^=salt;r->stream_key^=salt;r->cached_pair=UINT_MAX;
}
double spectral_path_random(PathRandom *r) {
    if(!r->qmc) {
        uint64_t dimension=++r->dimension;
        return RuntimeSpectral_Uniform64(r->key^(dimension*UINT64_C(0xd1b54a32d192ed03)));
    }
    unsigned dimension=r->dimension++;
    uint64_t key=mix(r->stream_key^((uint64_t)dimension*UINT64_C(0xd1b54a32d192ed03))^((uint64_t)r->event*UINT64_C(0xa0761d6478bd642f)));
    if(r->event>=1 && r->event<=4 && dimension<16)
        return transport_sample(r->sample_index,(r->event-1)*16+dimension,key);
    if(r->event || dimension>=2)
        return RuntimeSpectral_Uniform64(key^((uint64_t)r->sample_index*UINT64_C(0x94d049bb133111eb)));
    if(r->cached_pair!=0) {
        key=mix(r->stream_key);
        RuntimeNative3DSamplingContext s={.temporalSubpassIndex=(uint16_t)r->sample_index,
            .temporalSubpassCount=8192,.cameraPixelSampler=RUNTIME_CAMERA_PIXEL_PROGRESSIVE,
            .cameraSequenceSeed=(uint32_t)(key^(key>>32))};
        RuntimeNative3DSampling_PixelPosition(&s,0,0,&r->pair[0],&r->pair[1]);
        r->pair[0]+=.5;r->pair[1]+=.5;r->cached_pair=0;
    }
    return r->pair[dimension];
}
