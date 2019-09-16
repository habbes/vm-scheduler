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

    for (int i = 0; i < stats->numDomains; i++) {
        domain = GuestListDomainAt(guests, i);
        numStats = virDomainMemoryStats(domain, tempStats, MAX_STATS, 0);
        check(numStats > 0, "Could not get domain memory stats");

        stats->domainStats[i].max = (MemStatUnit) virDomainGetMaxMemory(domain);
        check(stats->domainStats[i].max > 0, "failed to get domain max memory");

        for (int j = 0; j < numStats; j++) {
            switch (tempStats[j].tag) {
                case VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON:
                    if (updateDeltas) {
                        stats->domainDeltas[i].actual = (MemStatUnit) tempStats[j].val - stats->domainStats[i].actual;
                    }
                    stats->domainStats[i].actual = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_UNUSED:
                    if (updateDeltas) {
                        stats->domainDeltas[i].unused = (MemStatUnit) tempStats[j].val - stats->domainStats[i].unused;
                    }
                    stats->domainStats[i].unused = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_USABLE:
                    if (updateDeltas) {
                        stats->domainDeltas[i].usable = (MemStatUnit) tempStats[j].val - stats->domainStats[i].usable;
                    }
                    stats->domainStats[i].usable = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_AVAILABLE:
                    if (updateDeltas) {
                        stats->domainDeltas[i].available = (MemStatUnit) tempStats[j].val - stats->domainStats[i].available;
                    }
                    stats->domainStats[i].available = (MemStatUnit) tempStats[j].val;
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
    printf("-- Total: %'llu\n", stats->hostStats.total);
    printf("-- Free: %'llu\n", stats->hostStats.free);
    puts("");

    for (int i = 0; i < stats->numDomains; i++) {
        printf("Domain %d stats\n", i);
        printf("-- Actual %'llu\n", stats->domainStats[i].actual);
        printf("-- Unused %'llu\n", stats->domainStats[i].unused);
        printf("-- Usable %'llu\n", stats->domainStats[i].usable);
        printf("-- Available %'llu\n", stats->domainStats[i].available);
        printf("-- Max: %'llu\n", stats->domainStats[i].max);
        printf("Domain %d deltas\n", i);
        printf("-- Actual %'llu\n", stats->domainDeltas[i].actual);
        printf("-- Unused %'llu\n", stats->domainDeltas[i].unused);
        printf("-- Usable %'llu\n", stats->domainDeltas[i].usable);
        printf("-- Available %'llu\n", stats->domainDeltas[i].available);
        puts("");
    }
}
