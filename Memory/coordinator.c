#include <stdlib.h>
#include "check.h"
#include "coordinator.h"
#include "allocplan.h"
#include "util.h"

#define LOW_UNUSED_THRESHOLD 0.2
#define SAFE_UNUSED_THRESHOLD 0.3
// minimum memory change in allocation plan to warrant de-allocation
#define MIN_CHANGE_FOR_DEALLOC 1024

#define MIN_HOST_MEMORY 500 * 1024

#define unusedPct(stats, dom) ((stats)->domainStats[(dom)].unused / (stats)->domainStats[(dom)].actual)

#define isUnusedBelowThreshold(stats, dom) (unusedPct(stats, dom) < LOW_UNUSED_THRESHOLD)

#define canDeallocate(stats, dom) (unusedPct(stats, dom) > SAFE_UNUSED_THRESHOLD)

int allocateStarvingGuests(AllocPlan *plan, MemStats *stats)
{
    int rt = 0;
    DomainMemStats *deltas = NULL;

    for (int d = 0; d < plan->numDomains; d++) {
        deltas = stats->domainDeltas + d;

        if (certainlyGreaterThan(0, deltas->unused) && isUnusedBelowThreshold(stats, d)) {
            // domain has used up more memory and is below threshold
            printf("Domain %d has used %'.2fkb and is below threshold (%.2f%%)\n",
                d, -deltas->unused, unusedPct(stats, d) * 100);
            rt = AllocPlanAddAlloc(plan, d, 2 * (-deltas->unused));
            check(rt == 0, "failed to add allocation to plan");
        }
    }

    return 0;

error:
    return -1;
}

int deallocateWastefulGuests(AllocPlan *plan, MemStats *stats)
{
    int rt = 0;
    DomainMemStats *deltas = NULL;
    for (int d = 0; d < plan->numDomains; d++) {
        deltas = stats->domainDeltas + d;

        if (deltas->unused > MIN_CHANGE_FOR_DEALLOC && canDeallocate(stats, d)) {
            // domain has released some memory and is above threshold
            printf("Domain %d has released %'.2fkb and is above threshold (%.2f%%)\n",
                d, deltas->unused, unusedPct(stats, d) * 100);
            rt = AllocPlanAddDealloc(plan, d, deltas->unused);
            check(rt == 0, "failed to add deallocation to plan");
        }
    }
    return 0;
error:
    return -1;
}

int deallocateSafeGuests(AllocPlan *plan, MemStats *stats)
{
    int rt = 0;
    MemStatUnit deallocMem = 0;
    deallocMem = AllocPlanDiff(plan);
    int candidates = 0;

    if (certainlyGreaterThan(deallocMem, MIN_CHANGE_FOR_DEALLOC)) {
        printf("Additional %'.2fkb needs to be freed, looking for candidates...\n", deallocMem);
        for (int d = 0; d < plan->numDomains; d++) {
            if (canDeallocate(stats, d)) {
               candidates += 1;
            }
        }
    }

    if (candidates > 0) {
        MemStatUnit deallocQuota = deallocMem / candidates;

        for (int d = 0; d < plan->numDomains; d++) {
            if (canDeallocate(stats, d)) {
               printf("Domain %d eligible for deallocating %'.2fkb\n", d, deallocQuota);
               rt = AllocPlanAddDealloc(plan, d, deallocQuota);
               check(rt == 0, "failed to add deallocation quota to domain");
            }
        }
    }
    else {
        printf("No additional domains eligible for deallocation\n");
    }

    return 0;
error:
    return -1;
}

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
    AllocPlan *plan = NULL;
    checkNull(stats);
    checkNull(guests);
    plan = AllocPlanCreate(guests->count);
    checkNull(plan);

    // get domains that need more memory
    rt = allocateStarvingGuests(plan, stats);
    check(rt == 0, "failed to allocate starving guests");

    // get candidates that are releasing memory
    rt = deallocateWastefulGuests(plan, stats);
    check(rt == 0, "failed to deallocate wasteful guests");

    // get remaining memory from candidates not using up their memory
    rt = deallocateSafeGuests(plan, stats);
    check(rt == 0, "failed to deallocate safe guests");
    
    rt = AllocPlanComputeNewSizes(plan, stats, stats->hostStats.total - MIN_HOST_MEMORY);
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