#include <stdlib.h>
#include "check.h"
#include "coordinator.h"
#include "allocplan.h"
#include "util.h"

#define LOW_UNUSED_THRESHOLD 0.2
#define SAFE_UNUSED_THRESHOLD 0.3
// minimum memory change in allocation plan to warrant de-allocation
#define MIN_CHANGE_FOR_DEALLOC 1000

#define unusedPct(stats, dom) ((stats)->domainStats[(dom)].unused / (stats)->domainStats[(dom)].actual)

#define isUnusedBelowThreshold(stats, dom) (unusedPct(stats, dom) < LOW_UNUSED_THRESHOLD)

#define canDeallocate(stats, dom) (unusedPct(stats, dom) > SAFE_UNUSED_THRESHOLD)


int executeAllocationPlan(AllocPlan *plan, MemStats *stats, GuestList *guests)
{
    int rt = 0;
    virDomainPtr domain;
    for (int i = 0; i < plan->numDomains; i++) {
        domain = GuestListDomainAt(guests, i);
        if (!almostEquals(plan->newSizes[i], stats->domainStats[i].actual)) {
            printf("Setting memory %'lukb for domain %d\n", plan->newSizes[i], i);
            rt = virDomainSetMemory(domain, plan->newSizes[i]);
            check(rt == 0, "failed to set memory for domain");
        }
    }

    return 0;
error:
    return -1;
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

        if (certainlyGreaterThan(0, deltas->unused) && isUnusedBelowThreshold(stats, d)) {
            // domain has used up more memory and is below threshold
            printf("Domain %d has used %'.2fkb and is below threshold (%.2f%%)\n",
                d, -deltas->unused, unusedPct(stats, d) * 100);
            rt = AllocPlanAddAlloc(plan, d, -deltas->unused);
            check(rt == 0, "failed to add allocation to plan");
        }
    }

    // get candidates that are releasing memory
    for (int d = 0; d < guests->count; d++) {
        deltas = stats->domainDeltas;

        if (certainlyGreaterThan(deltas->unused, 0) && canDeallocate(stats, d)) {
            // domain has released some memory and is above threshold
            printf("Domain %d has released %'.2fkb and is above threshold (%.2f%%)\n",
                d, deltas->unused, unusedPct(stats, d) * 100);
            rt = AllocPlanAddDealloc(plan, d, deltas->unused);
            check(rt == 0, "failed to add deallocation to plan");
        }
    }

    // get remaining memory from candidates not using up their memory
    deallocMem = AllocPlanDiff(plan);
    int extraDeallocCandidates = 0;
    printf("Additional %'.2fkb needs to be freed, looking for candidates...\n", deallocMem);
    if (certainlyGreaterThan(deallocMem, MIN_CHANGE_FOR_DEALLOC)) {
        for (int d = 0; d < guests->count; d++) {
            deltas = stats->domainDeltas;

            if (canDeallocate(stats, d)) {
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
               rt = AllocPlanAddDealloc(plan, d, deallocQuota);
               check(rt == 0, "failed to add deallocation quota to domain");
            }
        }
    }
    
    rt = AllocPlanComputeNewSizes(plan, stats);
    check(rt == 0, "failed to compute new memory sizes");
    rt = executeAllocationPlan(plan, stats, guests);
    check(rt == 0, "failed to execute allocation plan");

    rt = 0;
    goto final;

error:
    rt = -1;
final:
    AllocPlanFree(plan);
    return rt;
}