#include <stdlib.h>
#include <math.h>
#include "check.h"
#include "util.h"

int countOnBits(unsigned char byte, int maxBits)
{
    maxBits = maxBits > 8 ? 8 : maxBits;
    int bits = 0;
    int i = 0;
    for (i = 0; i < maxBits; i++) {
        bits += byte & 1;
        byte >>= 1;
    }
    return bits;
}

int almostEquals(double a, double b)
{
    double diff = a - b;
    return fabs(diff) <= EQUALITY_PRECISION;
}

int certainlyGreaterThan(double a, double b) {
    double diff = a - b;
    return diff > EQUALITY_PRECISION;
}
