#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <libvirt/libvirt.h>
#include "scheduler.h"
#include "util.h"

int getDomainToUnpinFromCpu(unsigned char *cpuMaps, int numDomains, unsigned char cpumask, int numCpus)
{
    int d = 0;
    int domain = -1;
    int maxPins = INT_MIN;
    int pins = 0;
    checkNull(cpuMaps);

    for (d = 0; d < numDomains; d++) {
        pins = countBits(cpuMaps[d], numCpus);
        if ((pins > maxPins ) && (isPinnedToCpu(cpuMaps[d], cpumask))) {
            maxPins = pins;
            domain = d;
        }
    }
    return domain;

error:
    return -1;
}

int getDomainToPinToCpu(unsigned char *cpuMaps, int numDomains, unsigned char cpumask, int numCpus)
{
    int d = 0;
    int domain = -1;
    int minPins = INT_MAX;
    int pins = 0;
    checkNull(cpuMaps);

    for (d = 0; d < numDomains; d++) {
        pins = countBits(cpuMaps[d], numCpus);
        if ((pins < minPins) && !(isPinnedToCpu(cpuMaps[d], cpumask))) {
            minPins = pins;
            domain = d;
        }
    }
    return domain;

error:
    return -1;
}

int cpuComparer(const void *cpu1Ptr, const void *cpu2Ptr, void *targetDiffs)
{
    int diff1 = ((int *)targetDiffs)[(*(int *)cpu1Ptr)];
    int diff2 = ((int *)targetDiffs)[(*(int *)cpu2Ptr)];
    return diff1 - diff2;
}

int sortCpusByDiffs(int *cpus, int *targetDiffs, int numCpus)
{
    checkNull(cpus);
    checkNull(targetDiffs);
    qsort_r(cpus, numCpus, sizeof(int), cpuComparer, (void *) targetDiffs);

    return 0;
error:
    return -1;
}

int repinCpus(virConnectPtr conn, CpuStats *stats, GuestList *guests, int *targetDiffs)
{
    int rt = 0;
    int targetDiff = 0;
    unsigned char cpumap = 0;
    unsigned char *newCpuMaps = NULL;
    int *cpus = NULL;
    int cpu = 0;
    virDomainPtr domain = NULL;
    unsigned char cpumask = 0;
    int opcount = 0;
    int targetOps = 0;

    checkNull(conn);
    checkNull(stats);
    checkNull(guests);
    checkNull(targetDiffs);

    newCpuMaps = calloc(guests->count, sizeof(unsigned char));
    checkMemAlloc(newCpuMaps);
    cpus = malloc(sizeof(int) * stats->numCpus);
    checkMemAlloc(cpus);

    for (int i = 0; i < stats->numCpus; i++) {
        cpus[i] = i;
    }


    rt = sortCpusByDiffs(cpus, targetDiffs, stats->numCpus);
    check(rt == 0, "failed to sort cpus");
    printf("sorted cpus:\n");
    for (int i = 0; i < stats->numCpus; i++) {
        printf("cpu %d:%d, ", cpus[i], targetDiffs[cpus[i]]);
    }
    puts("\n");

    // TODO consider directly modifying stats->cpuMaps instead of cloning it
    memcpy(newCpuMaps, stats->cpuMaps, sizeof(unsigned char) * stats->numDomains);

    for (int i = 0; i < stats->numCpus; i++)
    {
        cpu = cpus[i];
        opcount = 0;
        cpumask = getCpuMask(cpu);
        targetDiff = targetDiffs[cpu];
        targetOps = abs(targetDiff);

        while (opcount < targetOps) {
            if (targetDiff < 0) {
                int d = getDomainToUnpinFromCpu(newCpuMaps, guests->count, cpumask, stats->numCpus);
                check(d >= 0, "did not find domain to unpin");
                // unpin cpu
                newCpuMaps[d] = newCpuMaps[d] & (~cpumask);
                printf("to unpin domain %d from cpu %i, old map 0x%X new map 0x%X\n", d, cpu, cpumap, newCpuMaps[d]);
                ++opcount;
            }
            if (targetDiff > 0) {
                int d = getDomainToPinToCpu(newCpuMaps, guests->count, cpumask, stats->numCpus);
                check(d >= 0, "did not find domain to unpin");
                // pin cpu
                newCpuMaps[d] = newCpuMaps[d] | cpumask;
                printf("to pin domain %d to cpu %i, old map 0x%X new map 0x%X\n", d, cpu, cpumap, newCpuMaps[d]);
                ++opcount;
            }
        }
    }

    for (int d = 0; d < guests->count; d++) {
        domain = GuestListDomainAt(guests, d);
        cpumap = newCpuMaps[d];
        printf("domain %d new pin 0x%X - old 0x%X\n", d, newCpuMaps[d], stats->cpuMaps[d]);
        check(newCpuMaps[d] != 0, "did not assign any cpu to domain");
        rt = virDomainPinVcpu(domain, 0, &cpumap, 1);
        check(rt != -1, "failed to repin vcpu");
    }

    rt = 0;
    goto final;

error:
    rt = -1;
final:
    if (newCpuMaps) {
        free(newCpuMaps);
    }
    if (cpus) {
        free(cpus);
    }
    return rt;
}

int computeTargetDiffsUsingWeight(CpuStats *stats, int *targetDiffs)
{
    CpuStatsWeight_t targetWeight = 0.0;
    CpuStatsWeight_t cpuWeight = 0.0;
    CpuStatsWeight_t weightDiff = 0.0;

    checkNull(stats);
    checkNull(targetDiffs);

    targetWeight = 1.0 / stats->numCpus;

    for (int i = 0; i < stats->numCpus; i++) {
        cpuWeight = CpuStatsGetCpuWeight(stats, i);
        weightDiff = targetWeight - cpuWeight;
        targetDiffs[i] = (int) roundl(weightDiff * stats->numDomains);
        printf("cpu %d, vm diff %d\n", i, targetDiffs[i]);
    }

    return 0;

error:
    return -1;
}

int computeTargetDiffsUsingGuestCount(CpuStats *stats, int *targetDiffs)
{
    int pinnedDomains = 0;
    int targetCount = 0;

    checkNull(stats);
    checkNull(targetDiffs);

    targetCount = (int) roundl(stats->numDomains / stats->numCpus);

    for (int i = 0; i < stats->numCpus; i++) {
        pinnedDomains = CpuStatsCountDomainsOnCpu(stats, i);
        targetDiffs[i] = targetCount - pinnedDomains;
    }

    return 0;

error:
    return -1;
}

int allocateCpus(virConnectPtr conn, CpuStats *stats, GuestList *guests)
{
    int rt = 0;
    int *targetDiffs = NULL;

    checkNull(conn);
    checkNull(stats);
    checkNull(guests);

    targetDiffs = calloc(stats->numCpus, sizeof(int));
    checkMemAlloc(targetDiffs);

    // rt = computeTargetDiffsUsingWeight(stats, targetDiffs);
    rt = computeTargetDiffsUsingGuestCount(stats, targetDiffs);
    check(rt == 0, "could not compute target diffs");

    repinCpus(conn, stats, guests, targetDiffs);

    rt = 0;
    goto final;

error:
   rt = -1;
final:
    if (targetDiffs) {
        free(targetDiffs);
    }
    return rt;
}