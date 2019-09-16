#include <stdlib.h>
#include <stdio.h>
#include "memstats.h"
#include "check.h"

MemStats *MemStatsGet(virConnectPtr conn, GuestList *guests)
{
    MemStats *stats = NULL;
    virDomainMemoryStatStruct tempStats[MAX_STATS];
    int numStats = 0;
    virDomainPtr domain = NULL;
    stats = calloc(1, sizeof(MemStats));
    checkMemAlloc(stats);

    stats->numDomains = guests->count;
    stats->domainStats = calloc(guests->count, sizeof(DomainMemStats));
    checkMemAlloc(stats->domainStats);

    for (int i = 0; i < guests->count; i++) {
        domain = GuestListDomainAt(guests, i);
        numStats = virDomainMemoryStats(domain, tempStats, MAX_STATS, 0);
        check(numStats > 0, "Could not get domain memory stats");

        for (int j = 0; j < numStats; j++) {
            switch (tempStats[j].tag) {
                case VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON:
                    stats->domainStats[i].actual = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_UNUSED:
                    stats->domainStats[i].unused = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_USABLE:
                    stats->domainStats[i].usable = (MemStatUnit) tempStats[j].val;
                    break;
                case VIR_DOMAIN_MEMORY_STAT_AVAILABLE:
                    stats->domainStats[i].available = (MemStatUnit) tempStats[j].val;
                    break;
            }
        }
    }

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
        free(stats);
    }
}

void MemStatsPrint(MemStats *stats)
{
    if (!stats) {
        printf("Null stats pointer\n");
        return;
    }
    printf("Memory stats\n");

    for (int i = 0; i < stats->numDomains; i++) {
        printf("Domain %d stats\n", i);
        printf("-- Actual %llu\n", stats->domainStats[i].actual);
        printf("-- Unused %llu\n", stats->domainStats[i].unused);
        printf("-- Usable %llu\n", stats->domainStats[i].usable);
        printf("-- Available %llu\n", stats->domainStats[i].available);
    }
}