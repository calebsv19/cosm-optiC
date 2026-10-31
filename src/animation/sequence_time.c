#include "sequence_time_internal.h"
#include <limits.h>

static uint64_t gcd(uint64_t a, uint64_t b) {
    while (b) { uint64_t r = a % b; a = b; b = r; }
    return a;
}
static bool product(uint64_t a, uint64_t b, uint64_t *out) {
    if (b && a > (uint64_t)INT64_MAX / b) return false;
    *out = a * b;
    return true;
}
SequenceStatus SequenceTimeReduce(SequenceTime value, SequenceTime *out) {
    if (!out || !value.denominator) return SEQUENCE_INVALID_TIME;
    if (value.numerator > INT64_MAX || value.denominator > INT64_MAX)
        return SEQUENCE_NUMERIC_RANGE;
    uint64_t divisor = gcd(value.numerator, value.denominator);
    *out = (SequenceTime){value.numerator/divisor, value.denominator/divisor};
    return SEQUENCE_OK;
}
SequenceStatus SequenceTimeMultiply(SequenceTime a, SequenceTime b, SequenceTime *out) {
    SequenceStatus status = SequenceTimeReduce(a, &a);
    if (status != SEQUENCE_OK) return status;
    status = SequenceTimeReduce(b, &b);
    if (status != SEQUENCE_OK) return status;
    uint64_t x = gcd(a.numerator,b.denominator), y = gcd(b.numerator,a.denominator);
    SequenceTime result;
    if (!product(a.numerator/x,b.numerator/y,&result.numerator) ||
        !product(a.denominator/y,b.denominator/x,&result.denominator))
        return SEQUENCE_NUMERIC_RANGE;
    return SequenceTimeReduce(result,out);
}
static SequenceStatus sum(SequenceTime a, SequenceTime b, bool subtract,
                          SequenceTime *out) {
    SequenceStatus status = SequenceTimeReduce(a,&a);
    if (status != SEQUENCE_OK) return status;
    status = SequenceTimeReduce(b,&b);
    if (status != SEQUENCE_OK) return status;
    uint64_t g = gcd(a.denominator,b.denominator), left, right, denominator;
    if (!product(a.numerator,b.denominator/g,&left) ||
        !product(b.numerator,a.denominator/g,&right) ||
        !product(a.denominator,b.denominator/g,&denominator))
        return SEQUENCE_NUMERIC_RANGE;
    if (subtract ? left < right : left > (uint64_t)INT64_MAX-right)
        return subtract ? SEQUENCE_INVALID_TIME : SEQUENCE_NUMERIC_RANGE;
    return SequenceTimeReduce((SequenceTime){subtract ? left-right : left+right,
                                            denominator},out);
}
SequenceStatus SequenceTimeAdd(SequenceTime a, SequenceTime b, SequenceTime *out) {
    return sum(a,b,false,out);
}
SequenceStatus SequenceTimeSubtract(SequenceTime a, SequenceTime b, SequenceTime *out) {
    return sum(a,b,true,out);
}
int SequenceTimeCompare(SequenceTime a, SequenceTime b) {
    /* Continued fractions avoid overflow even for near-INT64_MAX fractions. */
    int sign = 1;
    for (;;) {
        uint64_t qa=a.numerator/a.denominator, qb=b.numerator/b.denominator;
        if (qa != qb) return sign * (qa < qb ? -1 : 1);
        uint64_t ra=a.numerator%a.denominator, rb=b.numerator%b.denominator;
        if (!ra || !rb) return !ra && !rb ? 0 : sign * (!ra ? -1 : 1);
        a=(SequenceTime){a.denominator,ra};
        b=(SequenceTime){b.denominator,rb};
        sign=-sign;
    }
}
