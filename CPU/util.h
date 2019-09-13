#ifndef util_h
#define util_h

#define isPinnedToCpu(cpuMap, targetCpuMask) (((cpuMap) & (targetCpuMask)) == (targetCpuMask))
#define getCpuMask(cpu) ((unsigned char) 1 << cpu)

/**
 * count the number of bits in `byte` that are set
 * to 1. Only the `maxBits` least significant
 * bits are considered. `maxBits` should be no more than 8.
 * 
 * @return the number of 1 bits found
 */
int countBits(unsigned char byte, int maxBits);

/**
 * sort the elements of the array `ids` in ascending order based on their
 * corresponding values in `values`. `values[i]` corresponds
 * to `ids[i]`. `count` is the number of elements in each array.
 * 
 * NOTE: `ids` elements will be re-ordered by the sort operation, but
 * `values` will remain intact.
 */
int sortIdsByValues(int *ids, int *values, int count);

#endif
