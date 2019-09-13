#include <stdlib.h>
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


int valueComparer(const void *id1, const void *id2, void *values)
{
    int val1 = ((int *)values)[(*(int *)id1)];
    int val2 = ((int *)values)[(*(int *)id2)];
    return val1 - val2;
}

int sortIdsByValues(int *ids, int *values, int count)
{
    checkNull(ids);
    checkNull(values);
    qsort_r(ids, count, sizeof(int), valueComparer, (void *) values);

    return 0;
error:
    return -1;
}
