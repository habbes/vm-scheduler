#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <libvirt/libvirt.h>
#include "scheduler.h"
#include "util.h"

int computeTargetCpuWeights(CpuStats *stats, CpuStatsUsage_t *targetWeights)
{
    CpuStatsUsage_t totalWeight = 0;
    CpuStatsUsage_t targetWeight = 0;

    checkNull(stats);
    checkNull(targetWeights);

    for (int i = 0; i < stats->numDomains; i++) {
        totalWeight += stats->domainUsages[i];
    }
    targetWeight = totalWeight / stats->numCpus;

    for (int i = 0; i < stats->numCpus; i++) {
        targetWeights[i] = targetWeight;
    }

    return 0;

error:
    return -1;
}

int getDomainToPinToCpu(unsigned char cpumask, unsigned char *newCpuMaps, CpuStatsUsage_t *targetWeights, CpuStats *stats)
{
    int d = 0;
    int curDomain = -1;
    CpuStatsUsage_t curWeight = 0;
    CpuStatsUsage_t weight = 0;
    
    checkNull(newCpuMaps);
    checkNull(stats);
    checkNull(targetWeights);

    for (d = 0; d < stats->numDomains; d++) {
        weight = stats->domainUsages[d];
        if (weight > targetWeights[d]) {
            continue;
        }
        if (isPinnedToCpu(newCpuMaps[d], cpumask)) {
            continue;
        }
        if (weight > curWeight) {
            curDomain = d;
            continue;
        }
        if ((weight == curWeight) && (
            (isPinnedToCpu(stats->cpuMaps[d], cpumask) > isPinnedToCpu(stats->cpuMaps[curDomain], cpumask))
            ||
            (countOnBits(newCpuMaps[d], stats->numCpus) < countOnBits(newCpuMaps[curDomain], stats->numCpus))
        )) {
            curDomain = d;
            continue;
        }
    }

    return curDomain;
error:
    return -1;
}

int updateNewDomainMapsForCpu(int cpu, unsigned char *newCpuMaps, CpuStatsUsage_t *targetWeights, CpuStats *stats)
{
    int domain = -1;
    unsigned char cpumask = getCpuMask(cpu);

    checkNull(newCpuMaps);
    checkNull(targetWeights);
    checkNull(stats);

    domain = getDomainToPinToCpu(cpumask, newCpuMaps, targetWeights, stats);
    if (domain < 0) {
        return -1;
    }

    newCpuMaps[domain] = newCpuMaps[domain] | cpumask;
    targetWeights[cpu] -= stats->domainUsages[domain];

    return 0;
error:
    return -1;
}

int updateCpuMaps(unsigned char *newCpuMaps, CpuStatsUsage_t *targetWeights, CpuStats *stats)
{
    int cpu = 0;
    int res = 0;
    int numFailed = 0;
    checkNull(newCpuMaps);
    checkNull(targetWeights);
    checkNull(stats);

    while (numFailed < stats->numCpus) {
        for (cpu = 0; cpu < stats->numCpus; cpu++) {
            res = updateNewDomainMapsForCpu(cpu, newCpuMaps, targetWeights, stats);
            if (res < 0) {
                numFailed++;
            }
        }
    }

    return 0;
error:
    return -1;
}

int pinNewCpuMaps(unsigned char *newCpuMaps, CpuStats *stats, GuestList *guests)
{
    int rt = 0;
    virDomainPtr domain = NULL;

    for (int d = 0; d < guests->count; d++) {
        domain = GuestListDomainAt(guests, d);
        if (newCpuMaps[d] != stats->cpuMaps[d]) {
            printf("domain %d new pin 0x%X - old 0x%X\n", d, newCpuMaps[d], stats->cpuMaps[d]);
            check(newCpuMaps[d] != 0, "did not assign any cpu to domain");
            rt = virDomainPinVcpu(domain, 0, newCpuMaps + d, 1);
            check(rt != -1, "failed to repin vcpu");
        }
    }

    return 0;
error:
    return -1;
}

int repinCpus(CpuStats *stats, GuestList *guests, CpuStatsUsage_t *targetWeights)
{
    int rt = 0;
    unsigned char *newCpuMaps = NULL;

    checkNull(stats);
    checkNull(guests);
    checkNull(targetWeights);

    newCpuMaps = calloc(guests->count, sizeof(unsigned char));
    checkMemAlloc(newCpuMaps);

    updateCpuMaps(newCpuMaps, targetWeights, stats);
    pinNewCpuMaps(newCpuMaps, stats, guests);
    
    rt = 0;
    goto final;

error:
    rt = -1;
final:
    if (newCpuMaps) {
        free(newCpuMaps);
    }
    return rt;
}

int allocateCpus(CpuStats *stats, GuestList *guests)
{
    int rt = 0;
    CpuStatsUsage_t *targetWeights = NULL;

    checkNull(stats);
    checkNull(guests);

    targetWeights = calloc(stats->numCpus, sizeof(CpuStatsUsage_t));
    checkMemAlloc(targetWeights);

    // rt = computeTargetDiffsUsingWeight(stats, targetDiffs);
    rt = computeTargetCpuWeights(stats, targetWeights);
    check(rt == 0, "could not compute target diffs");

    for (int i = 0; i < stats->numCpus; i++) {
        printf("cpu %d target weight %.2Lf\n", i, 100 * targetWeights[i]);
    }

    repinCpus(stats, guests, targetWeights);

    rt = 0;
    goto final;

error:
   rt = -1;
final:
    if (targetWeights) {
        free(targetWeights);
    }
    return rt;
}
