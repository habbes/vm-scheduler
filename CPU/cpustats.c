#include <stdlib.h>
#include <string.h>
#include "cpustats.h"
#include "util.h"

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
    stats->cpuMaps = calloc(domains, sizeof(unsigned char));
    checkMemAlloc(stats->cpuMaps);

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
        if (stats->cpuMaps) {
            free(stats->cpuMaps);
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

int CpuStatsCountDomainsOnCpu(CpuStats *stats, int cpu)
{
    CpuStatsCheckStatsArg(stats);
    CpuStatsCheckCpuArg(cpu);
    int count = 0;
    int d = 0;

    for (d = 0; d < stats->numDomains; d++) {
        if (isPinnedToCpu(stats->cpuMaps[d], getCpuMask(cpu))) {
            ++count;
        }
    }

    return count;

error:
    return -1;
}

int CpuStatsUpdateCpuMaps(CpuStats *stats, GuestList *guests)
{
    virDomainPtr domain = NULL;
    unsigned char cpumap = 0;
    int rt = 0;
    checkNull(stats);
    checkNull(guests);

    for (int i = 0; i < guests->count; i++) {
        domain = GuestListDomainAt(guests, i);
        rt = virDomainGetVcpuPinInfo(domain, 1, &cpumap, 1, 0);
        check(rt != -1, "failed to get vcpu pin info");
        stats->cpuMaps[i] = cpumap;
    }

    return 0;

error:
    return -1;
}

int updateStats(CpuStats *stats, GuestList *guests, int timeInterval)
{
    int nparams = 0;
    int d = 0; // domain iterator
    int c = 0; // cpu iterator
    int p = 0; // param iterator
    int paramPos = 0;
    unsigned long long prevTime = 0L;
    unsigned long long currTime = 0L;
    unsigned long long timeDiff = 0L;
    int rt = 0;
    virDomainPtr domain = NULL;
    virTypedParameterPtr params = NULL;

    check(stats, "stats is null");
    check(guests, "guests is null");
 
    nparams = virDomainGetCPUStats(GuestListDomainAt(guests, 0), NULL, 0, 0, 1, 0);
    check(nparams >= 0, "failed to get domain cpu params");
    params = calloc(stats->numCpus * nparams, sizeof(virTypedParameter));
    check(params, "failed to allocated params");

    rt = CpuStatsResetUsages(stats);
    check(rt == 0, "failed to reset usages");

    rt = CpuStatsUpdateCpuMaps(stats, guests);
    check(rt == 0, "failed to update cpu maps");

    for (d = 0; d < guests->count; d++) {
        domain = GuestListDomainAt(guests, d);
        virDomainGetCPUStats(domain, params, nparams, 0, stats->numCpus, 0);

        for (c = 0; c < stats->numCpus; c++) {
            for (p = 0; p < nparams; p++) {
                paramPos = nparams * c + p;

                if (strncmp(params[paramPos].field, "vcpu_time", VIR_TYPED_PARAM_FIELD_LENGTH) == 0) {
                    rt = CpuStatsGetTime(stats, c, d, &prevTime);
                    check(rt == 0, "failed to get cpu time from stats");
                    currTime = params[paramPos].value.ul;
                    timeDiff = prevTime > 0 ? currTime - prevTime : 0;
                    rt = CpuStatsAddUsage(stats, c, (CpuStatsUsage_t) timeDiff);
                    check(rt == 0, "failed to add cpu usage");
                    rt = CpuStatsAddDomainUsage(stats, d, (CpuStatsUsage_t) timeDiff);
                    check(rt == 0, "failed to add domain usage");
                    rt = CpuStatsSetTime(stats, c, d, currTime);
                    check(rt == 0, "failed to updated cpu time");
                }
            }
        }
    }

    rt = CpuStatsUsagesToPct(stats, timeInterval);
    check(rt == 0, "failed to updated usages");

    rt = 0;
    goto final;

error:
    rt = -1;
final:
    if (params) {
        free(params);
    }
    return rt;
}
