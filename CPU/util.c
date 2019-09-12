#include "util.h"

int countBits(unsigned char byte, int maxBits)
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