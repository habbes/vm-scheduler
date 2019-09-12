#include <stdlib.h>
#include <string.h>
#include "cpustats.h"

CpuStats *CpuStatsCreate(int cpus, int domains)
{
    CpuStats *stats = calloc(1, sizeof(CpuStats));
    checkMemAlloc(stats);
    stats->numCpus = cpus;
    stats->numDomains = domains;
    stats->usages = calloc(cpus, sizeof(CpuStatsUsage_t));
    checkMemAlloc(stats->usages);
    stats->cpuWeights = calloc(cpus, sizeof(CpuStatsWeight_t));
    checkMemAlloc(stats->cpuWeights);
    stats->times = calloc(domains * cpus, sizeof(CpuStatsTime_t));
    checkMemAlloc(stats->times);
    stats->domainUsages = calloc(domains, sizeof(CpuStatsUsage_t));
    checkMemAlloc(stats->domainUsages);

    return stats;

error:
    CpuStatsFree(stats);
    return NULL;
}

void CpuStatsFree(CpuStats *stats)
{
    if (stats) {
        if (stats->usages) {
            free(stats->usages);
        }
        if (stats->times) {
            free(stats->times);
        }
        if (stats->domainUsages) {
            free(stats->domainUsages);
        }
        if (stats->cpuWeights) {
            free(stats->cpuWeights);
        }
        free(stats);
    }
}

int CpuStatsSetTime(CpuStats *stats, int cpu, int domain, CpuStatsTime_t time)
{
    CpuStatsCheckArgs(stats, cpu, domain);
    *(stats->times + stats->numCpus * domain + cpu) = time;
    return 0;
error:
    return -1;
}

int CpuStatsGetTime(CpuStats *stats, int cpu, int domain, CpuStatsTime_t *timePtr)
{
    CpuStatsCheckArgs(stats, cpu, domain);
    *timePtr = *(stats->times + stats->numCpus * domain + cpu);
    return 0;
error:
    return -1;
}

int CpuStatsResetUsages(CpuStats *stats)
{
    check(stats, "stats is null");
    memset(stats->usages, 0, sizeof(CpuStatsUsage_t) * stats->numCpus);
    memset(stats->domainUsages, 0, sizeof(CpuStatsUsage_t) * stats->numDomains);
    memset(stats->cpuWeights, 0, sizeof(CpuStatsWeight_t) * stats->numCpus);
    return 0;
error:
    return 1;
}

int CpuStatsAddUsage(CpuStats *stats, int cpu, CpuStatsUsage_t usage)
{
    CpuStatsCheckStatsArg(stats);
    CpuStatsCheckCpuArg(cpu);

    stats->usages[cpu] = stats->usages[cpu] + usage;
    return 0;
error:
    return -1;
}

int CpuStatsAddDomainUsage(CpuStats *stats, int domain, CpuStatsUsage_t usage)
{
    CpuStatsCheckStatsArg(stats);
    CpuStatsCheckDomainArg(domain);

    stats->domainUsages[domain] = stats->domainUsages[domain] + usage;
    return 0;

error:
    return -1;
}

int CpuStatsUsagesToPct(CpuStats *stats, double timeInterval)
{
    int i = 0;
    CpuStatsUsage_t totalUsage = 0;
    CpuStatsCheckStatsArg(stats);

    for (i = 0; i < stats->numCpus; i++) {
        stats->usages[i] = stats->usages[i] / 1e9;
        if (timeInterval > 0) {
            stats->usages[i] = stats->usages[i] / timeInterval;
        }
        totalUsage = totalUsage + stats->usages[i];
    }

    for (i = 0; i < stats->numCpus; i++) {
        stats->cpuWeights[i] = totalUsage != 0 ?
            (CpuStatsWeight_t) (stats->usages[i] / totalUsage) : 1.0 / totalUsage;
    }

    for (i = 0; i < stats->numDomains; i++) {
        stats->domainUsages[i] = stats->domainUsages[i] / 1e9;
        if (timeInterval > 0) {
            stats->domainUsages[i] = stats->domainUsages[i] / timeInterval;
        }
    }

    return 0;

error:
    return -1;
}

int CpuStatsPrint(CpuStats *stats)
{
    // CpuStatsTime_t cpuTime = 0;
    check(stats, "Stats cannot be null");

    for (int c = 0; c < stats->numCpus; c++) {
        printf("cpu %d usage: %.2Lf\n", c, 100 * stats->usages[c]);
        printf("- cpu weight %.2Lf\n", stats->cpuWeights[c]);
    }

    for (int i = 0; i < stats->numDomains; i++) {
        printf("domain %d\n", i);
        printf("domain usage: %.2Lf\n", 100 * stats->domainUsages[i]);
        // for (int c = 0; c < stats->numCpus; c++) {
        //     cpuTime = *(stats->times + stats->numCpus * i + c);
        //     printf("-- cpuTime %d: %llu\n", c, cpuTime);
        // }
    }

    return 0;
error:
    return -1;
}



