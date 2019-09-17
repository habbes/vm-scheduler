#ifndef util_h
#define util_h

#define EQUALITY_PRECISION 100

/**
 * count the number of bits in `byte` that are set
 * to 1. Only the `maxBits` least significant
 * bits are considered. `maxBits` should be no more than 8.
 * 
 * @return the number of 1 bits found
 */
int countOnBits(unsigned char byte, int maxBits);

int almostEquals(double a, double b);
int certainlyGreaterThan(double a, double b);

#endif
