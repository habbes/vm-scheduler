#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memstats.h"
#include "check.h"

int MemStatsUpdateHostStats(virConnectPtr conn, MemStats *stats)
{
    int nparams = 0;
    int rt = 0;
    int cellNum = VIR_NODE_MEMORY_STATS_ALL_CELLS;
    int fieldLength = VIR_NODE_MEMORY_STATS_FIELD_LENGTH;
    virNodeMemoryStatsPtr tempStats = NULL;
    rt = virNodeGetMemoryStats(conn, cellNum, NULL, &nparams, 0);
    check(rt == 0, "failed to get node memory stats params");
    tempStats = calloc(nparams, sizeof(virNodeMemoryStats));
    checkMemAlloc(tempStats);
    rt = virNodeGetMemoryStats(conn, cellNum, tempStats, &nparams, 0);
    check(rt == 0, "failed to get node memory stats");

    for (int i = 0; i < nparams; i++) {
        if (strncmp(tempStats[i].field, "total", fieldLength) == 0) {
            stats->hostStats.total = (MemStatUnit) tempStats[i].value;
        }
        else if (strncmp(tempStats[i].field, "free", fieldLength) == 0) {
            stats->hostStats.free = (MemStatUnit) tempStats[i].value;
        }
    }

    rt = 0;
    goto final;

error:
    rt = -1;
final:
    free(tempStats);
    return rt;
}

int MemStatsUpdateDomainStats(virConnectPtr conn, GuestList *guests, MemStats *stats, int updateDeltas)
{
    int numStats = 0;
    virDomainPtr domain = NULL;
    virDomainMemoryStatStruct tempStats[MAX_STATS];
    DomainMemStats *deltas;
    DomainMemStats *domainStats;

    for (int i = 0; i < stats->numDomains; i++) {
        domain = GuestListDomainAt(guests, i);
        numStats = virDomainMemoryStats(domain, tempStats, MAX_STATS, 0);
        deltas = stats->domainDeltas + i;
        domainStats = stats->domainStats + i;


        check(numStats > 0, "Could not get domain memory stats");

        stats->domainStats[i].max = (MemStatUnit) virDomainGetMaxMemory(domain);
        check(stats->domainStats[i].max > 0, "failed to get domain max memory");

        for (int j = 0; j < numStats; j++) {
            switch (tempStats[j].tag) {
                case VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON:
                    if (updateDeltas) {
                        deltas->actual = (MemStatUnit) tempStats[j].val - domainStats->actual;
                    }
                    domainStats->actual = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_UNUSED:
                    if (updateDeltas) {
                        deltas->unused = (MemStatUnit) tempStats[j].val - domainStats->unused;
                    }
                    domainStats->unused = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_USABLE:
                    if (updateDeltas) {
                        deltas->usable = (MemStatUnit) tempStats[j].val - domainStats->usable;
                    }
                    domainStats->usable = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_AVAILABLE:
                    if (updateDeltas) {
                        deltas->available = (MemStatUnit) tempStats[j].val - domainStats->available;
                    }
                    domainStats->available = (MemStatUnit) tempStats[j].val;
                    break;
            }
        }
    }
    return 0;

error:
    return -1;
}

MemStats *MemStatsCreate(virConnectPtr conn, GuestList *guests)
{
    MemStats *stats = NULL;
    stats = calloc(1, sizeof(MemStats));
    checkMemAlloc(stats);

    stats->numDomains = guests->count;
    stats->domainStats = calloc(guests->count, sizeof(DomainMemStats));
    checkMemAlloc(stats->domainStats);
    stats->domainDeltas = calloc(guests->count, sizeof(DomainMemStats));
    checkMemAlloc(stats->domainDeltas);

    return stats;
error:
    MemStatsFree(stats);
    return NULL;
}

void MemStatsFree(MemStats *stats)
{
    if (stats) {
        if (stats->domainStats) {
            free(stats->domainStats);
        }
        if (stats->domainDeltas) {
            free(stats->domainDeltas);
        }
        free(stats);
    }
}

int MemStatsInit(MemStats *stats, virConnectPtr conn, GuestList *guests)
{
    return MemStatsUpdate(stats, conn, guests, 0);
}

int MemStatsUpdate(MemStats *stats, virConnectPtr conn, GuestList *guests, int updateDeltas)
{
    int rt = 0;
    checkNull(stats);
    checkNull(conn);
    checkNull(guests);

    rt = MemStatsUpdateHostStats(conn, stats);
    check(rt == 0, "failed to update host stats");

    rt = MemStatsUpdateDomainStats(conn, guests, stats, updateDeltas);
    check(rt == 0, "failed to update domain stats");

    return 0;

error:
    return -1;
}

void MemStatsPrint(MemStats *stats)
{
    if (!stats) {
        printf("Null stats pointer\n");
        return;
    }
    printf("Memory stats\n");
    
    printf("Host stats\n");
    printf("-- Total: %'2.f\n", stats->hostStats.total);
    printf("-- Free: %'2.f\n", stats->hostStats.free);
    puts("");

    for (int i = 0; i < stats->numDomains; i++) {
        printf("Domain %d stats\n", i);
        printf("-- Actual: %'.2f\n", stats->domainStats[i].actual);
        printf("-- Unused: %'.2f\n", stats->domainStats[i].unused);
        printf("-- Usable: %'.2f\n", stats->domainStats[i].usable);
        printf("-- Available: %'.2f\n", stats->domainStats[i].available);
        printf("-- Max: %'.2f\n", stats->domainStats[i].max);
        printf("Domain %d deltas\n", i);
        printf("-- Actual: %'.2f\n", stats->domainDeltas[i].actual);
        printf("-- Unused: %'.2f\n", stats->domainDeltas[i].unused);
        printf("-- Usable: %'.2f\n", stats->domainDeltas[i].usable);
        printf("-- Available: %'.2f\n", stats->domainDeltas[i].available);
        puts("");
    }
}
