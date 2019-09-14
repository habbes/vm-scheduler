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

int getDomainToPinToCpu(unsigned char cpumask, unsigned char *newCpuMaps, CpuStatsUsage_t targetWeight, CpuStats *stats)
{
    int d = 0;
    int curDomain = -1;
    CpuStatsUsage_t curWeight = -1;
    CpuStatsUsage_t weight = 0;
    int curPins = INT_MAX;
    
    checkNull(newCpuMaps);
    checkNull(stats);

    for (d = 0; d < stats->numDomains; d++) {
        weight = stats->domainUsages[d];
        curWeight = curDomain > -1 ? stats->domainUsages[curDomain] : -1;
        curPins = curDomain > -1 ? countOnBits(newCpuMaps[curDomain], stats->numCpus) : INT_MAX;
        
        // skip domains with large weight than cpu target weight
        if (certainlyGreaterThan((double) weight, (double) targetWeight)) {
            continue;
        }
        if (isPinnedToCpu(newCpuMaps[d], cpumask)) {
            continue;
        }
        // take the domain with the higher weight
        if (certainlyGreaterThan((double) weight, (double) curWeight) &&
            (countOnBits(newCpuMaps[d], stats->numCpus) <= curPins)
        ) {
            curDomain = d;
            continue;
        }
        // if domains have same weight, prefer domain which is already pinned to the cpu
        // to avoid pin changes, or the domain with fewer new mappings to other cpus
        if ((almostEquals((double) weight, (double) curWeight)) && (
            (isPinnedToCpu(stats->cpuMaps[d], cpumask) > isPinnedToCpu(stats->cpuMaps[curDomain], cpumask))
            ||
            (countOnBits(newCpuMaps[d], stats->numCpus) < countOnBits(newCpuMaps[curDomain], stats->numCpus))
        )) {
            curDomain = d;
            continue;
        }

        if (curDomain < 0) {
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

    domain = getDomainToPinToCpu(cpumask, newCpuMaps, targetWeights[cpu], stats);
    if (domain < 0) {
        return -1;
    }

    newCpuMaps[domain] = newCpuMaps[domain] | cpumask;
    targetWeights[cpu] -= stats->domainUsages[domain];
     printf("cpu %d receives domain %d, new weight %.2Lf, domain weight %.2Lf, new map 0x%X\n",
        cpu, domain, targetWeights[cpu], stats->domainUsages[domain], newCpuMaps[domain]);

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
            printf("target weight to fill %d:%.2Lf, res %d\n", cpu, targetWeights[cpu], res);
            // res = -1;
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

    rt = computeTargetCpuWeights(stats, targetWeights);
    check(rt == 0, "could not compute target diffs");

    for (int i = 0; i < stats->numCpus; i++) {
        printf("cpu %d target weight %.2Lf\n", i, targetWeights[i]);
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
