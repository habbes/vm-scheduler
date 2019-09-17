#include <stdlib.h>
#include "check.h"
#include "coordinator.h"
#include "allocplan.h"

#define LOW_UNUSED_THRESHOLD 0.2
#define SAFE_UNUSED_THRESHOLD 0.3

#define unusedPct(stats, dom) ((stats)->domainStats[(dom)].unused / (stats)->domainStats[(dom)].actual)

#define isUnusedBelowThreshold(stats, dom) (unusedPct(stats, dom) < LOW_UNUSED_THRESHOLD)

#define canDeallocate(stats, dom) (unusedPct(stats, dom) > SAFE_UNUSED_THRESHOLD)




int changeMemorySize(int d, MemStatUnit sizeChange, MemStats *stats, GuestList *guests)
{
    virDomainPtr domain = GuestListDomainAt(guests, d);
    unsigned long newSize = (unsigned long) (stats->domainStats[d].actual + sizeChange);
    printf("Setting new size %'lukb (diff %'.2fkb) for domain %d\n", newSize, sizeChange, d);
    return  virDomainSetMemory(domain, newSize);
}

int executeAllocationPlan(AllocPlan *plan, MemStats *stats, GuestList *guests)
{
    int domain = -1;
    MemStatUnit sizeChange = 0;
    for (int i = 0; i < plan->numDealloc; i++) {
        domain = plan->toDealloc[i].domain;
        sizeChange = plan->toDealloc[i].size;
        changeMemorySize(domain, -sizeChange, stats, guests);
    }
    for(int i = 0; i < plan->numAlloc; i++) {
        domain = plan->toAlloc[i].domain;
        sizeChange = plan->toAlloc[i].size;
        changeMemorySize(domain, sizeChange, stats, guests);
    }

    return 0;
}


int reallocateMemory(MemStats *stats, GuestList *guests)
{
    int rt = 0;
    DomainMemStats *deltas;
    AllocPlan *plan = NULL;
    MemStatUnit deallocMem = 0;
    checkNull(stats);
    checkNull(guests);
    plan = AllocPlanCreate(guests->count);
    checkNull(plan);

    // get domains that need more memory
    for (int d = 0; d < guests->count; d++) {
        deltas = stats->domainDeltas + d;

        if (deltas->unused < 0 && isUnusedBelowThreshold(stats, d)) {
            // domain has used up more memory and is below threshold
            printf("Domain %d has used %'.2fkb and is below threshold (%.2f%%)\n",
                d, -deltas->unused, unusedPct(stats, d) * 100);
            AllocPlanAddAlloc(plan, d, -deltas->unused);
        }
    }

    // get candidates that are releasing memory
    for (int d = 0; d < guests->count; d++) {
        deltas = stats->domainDeltas;

        if (deltas->unused > 0 && canDeallocate(stats, d)) {
            // domain has released some memory and is above threshold
            printf("Domain %d has released %'.2fkb and is above threshold (%.2f%%)\n",
                d, deltas->unused, unusedPct(stats, d) * 100);
            AllocPlanAddDealloc(plan, d, deltas->unused);
        }
    }

    // get remaining memory from candidates not using up their memory
    deallocMem = AllocPlanDiff(plan);
    int extraDeallocCandidates = 0;
    printf("Additional %'.2fkb needs to be freed, looking for candidates...\n", deallocMem);
    if (deallocMem > 0) {
        for (int d = 0; d < guests->count; d++) {
            deltas = stats->domainDeltas;

            if (canDeallocate(stats, d)) {
                // domain has released some memory and is above threshold
               extraDeallocCandidates += 1;
            }
        }
        if (extraDeallocCandidates == 0) {
            printf("No additional domains eligible for deallocation\n");
        }
        MemStatUnit deallocQuota = deallocMem / extraDeallocCandidates;

        for (int d = 0; d < guests->count; d++) {
            deltas = stats->domainDeltas;

            if (canDeallocate(stats, d)) {
               printf("Domain %d eligible for deallocating %'.2fkb\n", d, deallocQuota);
               AllocPlanAddDealloc(plan, d, deallocQuota);
            }
        }
    }
    
    executeAllocationPlan(plan, stats, guests);

    rt = 0;
    goto final;

error:
    rt = -1;
final:
    AllocPlanFree(plan);
    return rt;
}