#include "render/fast_rng.h"
#include "render/runtime_spectral_observer.h"
#include "render/runtime_spectral_profile.h"
#include "test_support.h"

#include <math.h>
#include <stdio.h>

static void near_value(const char *label, double value, double expected, double tolerance) {
    assert_true(label, isfinite(value) && fabs(value - expected) <= tolerance);
}

static void test_measure_and_grid(void) {
    RuntimeSpectralKnot knots[401];
    RuntimeSpectralLine red = {630, 2};
    const unsigned steps[] = {10, 5, 1};
    for (size_t k = 0; k < 3; ++k) {
        size_t count = 400 / steps[k] + 1;
        for (size_t i = 0; i < count; ++i)
            knots[i] = (RuntimeSpectralKnot){380 + i * steps[k], 0.02};
        RuntimeSpectralProfile *mixed = RuntimeSpectral_Prepare(knots, count, &red, 1);
        near_value("mixed total", RuntimeSpectral_TotalWatts(mixed), 10, 1e-12);
        double total = 0, band = 0;
        size_t line_hits = 0;
        for (size_t i = 0; i < 10000; ++i) {
            RuntimeSpectralDraw d;
            double watts;
            if (!RuntimeSpectral_Sample(mixed, (i + 0.5) / 10000, &d) ||
                !RuntimeSpectral_EmissionWatts(mixed, d, 1, 10000, &watts)) {
                assert_true("mixed sampling", false);
                break;
            }
            total += watts;
            if (d.nm >= 625 && d.nm <= 635)
                band += watts;
            if (d.measure == SPECTRAL_LINE) {
                ++line_hits;
                if (d.nm != 630) {
                    assert_true("unshifted atom", false);
                    break;
                }
            }
        }
        near_value("flat sensor", total, 10, 1e-10);
        near_value("top hat", band, 2.2, 1e-10);
        assert_true("line count", line_hits == 2000);
        printf("spectral mixed grid=%u nm flat=%.9f W band=%.9f W\n", steps[k], total, band);
        RuntimeSpectral_Free(mixed);
    }
}

static void test_observer(void) {
    /* Reference quadrature used the original CIE CSV at 0.001 vacuum-nm
     * intervals. Exercise a different resolution, plus a known air-line row. */
    Vec3 integral = vec3(0, 0, 0);
    for (unsigned i = 0; i < 160000; ++i)
        integral = vec3_add(integral, vec3_scale(RuntimeSpectral_CIE1931(380 + (i + 0.5) * 0.0025), 0.0025));
    near_value("CIE X integral", integral.x, 106.88307286077, 1e-7);
    near_value("CIE Y integral", integral.y, 106.88495010458, 1e-7);
    near_value("CIE Z integral", integral.z, 106.87176020069, 1e-7);
    Vec3 red = RuntimeSpectral_CIE1931(630.1742274323085);
    near_value("red X", red.x, 0.6424, 1e-12);
    near_value("red Y", red.y, 0.265, 1e-12);
    near_value("red Z", red.z, 0.00004999999, 1e-12);
    near_value("CIE outside domain", RuntimeSpectral_CIE1931(379).y, 0, 0);
    assert_true("CIE upper edge", RuntimeSpectral_CIE1931(780).y > 0);
    printf("spectral 1 W/nm raw XYZ=(%.10f, %.10f, %.10f)\n", integral.x, integral.y, integral.z);
}

static void test_proposals(void) {
    RuntimeSpectralKnot white[] = {{380, 1}, {780, 1}};
    RuntimeSpectralKnot slope[] = {{380, 0.5}, {780, 1.5}};
    RuntimeSpectralProfile *source = RuntimeSpectral_Prepare(white, 2, NULL, 0);
    RuntimeSpectralProfile *proposal = RuntimeSpectral_Prepare(slope, 2, NULL, 0);
    enum { seeds = 8, samples = 65536, total_trials = seeds * samples };
    /* Total plus four 100 nm receivers, including zero-valued trials. */
    double sum[5] = {0}, squares[5] = {0};
    assert_true("coverage", RuntimeSpectral_Covers(proposal, source));
    double centroid = 0;
    for (unsigned j = 0; j < 400; ++j) {
        RuntimeSpectralDraw d;
        double watts;
        if (!RuntimeSpectral_Sample(source, (j + 0.5) / 400, &d) ||
            !RuntimeSpectral_EmissionWatts(source, d, 1, 400, &watts)) {
            assert_true("white source", false);
            break;
        }
        near_value("equal flux", watts, 1, 1e-14);
        centroid += d.nm * watts / 400;
    }
    near_value("white centroid", centroid, 580, 1e-11);
    for (unsigned seed = 1; seed <= seeds; ++seed) {
        FastRNG rng;
        FastRNGSeed(&rng, seed, seed);
        for (size_t i = 0; i < samples; ++i) {
            RuntimeSpectralDraw d;
            double watts;
            if (!RuntimeSpectral_Sample(proposal, FastRNGNextDouble(&rng), &d) ||
                !RuntimeSpectral_EmissionWatts(source, d, 1, total_trials, &watts)) {
                assert_true("importance sampling", false);
                break;
            }
            unsigned band = 1 + (unsigned)fmin(3, (d.nm - 380) / 100);
            sum[0] += watts;
            squares[0] += watts * watts;
            sum[band] += watts;
            squares[band] += watts * watts;
        }
    }
    for (unsigned k = 0; k < 5; ++k) {
        double expected = k == 0 ? 400 : 100;
        double stderr_w =
            sqrt(fmax(0, (total_trials * squares[k] - sum[k] * sum[k]) / (total_trials - 1)));
        near_value("six standard errors", sum[k], expected, 6 * stderr_w + 1e-9);
        near_value("absolute error limit", sum[k], expected, 4);
        printf("spectral white channel=%u estimated=%.8f W stderr=%.8f W\n", k, sum[k], stderr_w);
    }
    RuntimeSpectral_Free(proposal);
    RuntimeSpectral_Free(source);
}

static void test_narrowband_and_mixed_proposal(void) {
    /* A 2 nm wide triangular source has a known piecewise-square CDF. */
    RuntimeSpectralKnot led[] = {{629, 0}, {630, 2}, {631, 0}};
    RuntimeSpectralLine lines[] = {{480, 1}, {630, 3}};
    RuntimeSpectralLine proposal_lines[] = {{480, 3}, {630, 1}};
    RuntimeSpectralProfile *source = RuntimeSpectral_Prepare(led, 3, lines, 2);
    RuntimeSpectralProfile *proposal = RuntimeSpectral_Prepare(led, 3, proposal_lines, 2);
    RuntimeSpectralProfile *continuous = RuntimeSpectral_Prepare(led, 3, NULL, 0);
    assert_true("same mixed support", RuntimeSpectral_Covers(proposal, source));
    assert_true("missing line rejected", !RuntimeSpectral_Covers(continuous, source));
    RuntimeSpectralDraw d;
    if (RuntimeSpectral_Sample(continuous, 0.125, &d)) {
        near_value("rising LED quantile", d.nm, 629.5, 1e-12);
    } else
        assert_true("rising draw", false);
    if (RuntimeSpectral_Sample(continuous, 0.875, &d)) {
        near_value("falling LED quantile", d.nm, 630.5, 1e-12);
    } else
        assert_true("falling draw", false);
    double total = 0, blue = 0, red = 0, moment = 0;
    Vec3 xyz = vec3(0, 0, 0);
    for (size_t i = 0; i < 60000; ++i) {
        double watts = 0;
        if (!RuntimeSpectral_Sample(proposal, (i + 0.5) / 60000, &d) ||
            !RuntimeSpectral_EmissionWatts(source, d, 1, 60000, &watts)) {
            assert_true("mixed importance sampling", false);
            break;
        }
        total += watts;
        xyz = vec3_add(xyz, vec3_scale(RuntimeSpectral_CIE1931(d.nm), watts));
        if (d.measure == SPECTRAL_CONTINUUM)
            moment += d.nm * watts;
        else if (d.nm == 480)
            blue += watts;
        else if (d.nm == 630)
            red += watts;
    }
    near_value("importance corrected blue mass", blue, 1, 1e-10);
    near_value("importance corrected red mass", red, 3, 1e-10);
    near_value("mixed power", total, 6, 1e-10);
    near_value("finite bandwidth centroid", moment / 2, 630, 1e-9);
    /* Independent 0.0001 vacuum-nm quadrature plus both exact lines. */
    Vec3 expected = vec3(3.32754693898, 1.47259952328, 0.81906382339);
    near_value("mixed sensor X", xyz.x, expected.x, 1e-7);
    near_value("mixed sensor Y", xyz.y, expected.y, 1e-7);
    near_value("mixed sensor Z", xyz.z, expected.z, 1e-7);
    printf("spectral mixed two-line plus LED: %.9f W XYZ=(%.9f, %.9f, %.9f)\n", total, xyz.x, xyz.y,
           xyz.z);
    RuntimeSpectral_Free(continuous);
    RuntimeSpectral_Free(proposal);
    RuntimeSpectral_Free(source);
}

static void test_boundaries(void) {
    RuntimeSpectralKnot triangle[] = {{380, 0}, {580, 2}, {780, 0}};
    RuntimeSpectralLine pair[] = {{480, 1}, {630, 3}};
    RuntimeSpectralProfile *both = RuntimeSpectral_Prepare(triangle, 3, pair, 2);
    RuntimeSpectralProfile *only_lines = RuntimeSpectral_Prepare(NULL, 0, pair, 2);
    RuntimeSpectralProfile *disabled = RuntimeSpectral_Prepare(NULL, 0, NULL, 0);
    RuntimeSpectralDraw d = {0};
    assert_true("empty prepared", disabled != NULL && !RuntimeSpectral_Sample(disabled, 0, &d));
    assert_true("continuous support required", !RuntimeSpectral_Covers(only_lines, both));
    assert_true("both line masses covered", RuntimeSpectral_Covers(both, only_lines));
    near_value("triangle plus two lines", RuntimeSpectral_TotalWatts(both), 404, 0);
    assert_true("rising zero endpoint", RuntimeSpectral_Sample(both, 0, &d) && d.probability > 0);
    assert_true("last atom", RuntimeSpectral_Sample(both, nextafter(1, 0), &d) && d.nm == 630);
    assert_true("reject endpoint 1", !RuntimeSpectral_Sample(both, 1, &d));
    assert_true("reject nan input", !RuntimeSpectral_Sample(both, NAN, &d));
    near_value("absent atom", RuntimeSpectral_Value(both, nextafter(630, 631), SPECTRAL_LINE), 0, 0);
    /* Two emitters with 404 and 4 W, selected equiprobably. Each is sampled
     * conditionally by power, giving the same compensated weight at every λ. */
    double w1 = 0, w2 = 0;
    assert_true("joint first", RuntimeSpectral_Sample(both, 0.25, &d) &&
                                   RuntimeSpectral_EmissionWatts(both, d, 0.5, 2, &w1));
    assert_true("joint second", RuntimeSpectral_Sample(only_lines, 0.8, &d) &&
                                    RuntimeSpectral_EmissionWatts(only_lines, d, 0.5, 2, &w2));
    near_value("joint flux", w1 + w2, 408, 1e-10);
    assert_true("no selected emitter", !RuntimeSpectral_EmissionWatts(NULL, d, 0, 2, &w1));
    assert_true("invalid source PDF", !RuntimeSpectral_EmissionWatts(both, d, 0, 2, &w1));
    triangle[1].nm = 380;
    assert_true("duplicate knot", RuntimeSpectral_Prepare(triangle, 3, NULL, 0) == NULL);
    pair[1].watts = -1;
    assert_true("negative power", RuntimeSpectral_Prepare(NULL, 0, pair, 2) == NULL);
    RuntimeSpectral_Free(both);
    RuntimeSpectral_Free(only_lines);
    RuntimeSpectral_Free(disabled);
}

int run_test_runtime_spectral(void) {
    int before = test_support_failures();
    test_measure_and_grid();
    test_observer();
    test_proposals();
    test_boundaries();
    test_narrowband_and_mixed_proposal();
    return test_support_failures() - before;
}
