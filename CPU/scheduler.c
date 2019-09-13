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

int allocateCpus(virConnectPtr conn, CpuStats *stats, GuestList *guests)
{
    int rt = 0;
    CpuStatsUsage_t *targetWeights = NULL;

    checkNull(conn);
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

    // repinCpus(conn, stats, guests, targetDiffs);

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