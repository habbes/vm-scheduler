#include <stdlib.h>
#include <math.h>
#include "check.h"
#include "coordinator.h"
#include "allocplan.h"
#include "util.h"

#define LOW_UNUSED_THRESHOLD 0.2
#define SAFE_UNUSED_THRESHOLD 0.3
// minimum memory change in allocation plan to warrant de-allocation
#define MIN_CHANGE_FOR_DEALLOC 1024
#define MIN_GUEST_MEMORY 100 * 1024
#define MIN_HOST_MEMORY 200 * 1024
#define MAX_FREE_MEMORY 300 * 1024

#define unusedPct(stats, dom) ((stats)->domainStats[(dom)].unused / (stats)->domainStats[(dom)].actual)

#define isUnusedBelowThreshold(stats, dom) (unusedPct(stats, dom) < LOW_UNUSED_THRESHOLD || \
    MemStatsUnused(stats, dom) <= MIN_GUEST_MEMORY)

#define canDeallocate(stats, dom) (unusedPct(stats, dom) > SAFE_UNUSED_THRESHOLD) && \
    !(isUnusedBelowThreshold(stats, dom))

int isWasteful(MemStats *stats, int dom)
{
    return stats->domainStats[dom].unused >= MAX_FREE_MEMORY &&
        stats->domainDeltas[dom].unused >= 0;
}

int allocateStarvingGuests(AllocPlan *plan, MemStats *stats)
{
    int rt = 0;
    double threshold = 0;
    double distToThresh = 0;
    DomainMemStats *deltas = NULL;

    for (int d = 0; d < plan->numDomains; d++) {
        deltas = stats->domainDeltas + d;
        threshold = unusedPct(stats, d) * MemStatsActual(stats, d);
        threshold = threshold > MIN_GUEST_MEMORY ? threshold : MIN_GUEST_MEMORY;

        if (certainlyGreaterThan(0, deltas->unused) && isUnusedBelowThreshold(stats, d)) {
            // domain has used up more memory and is below threshold
            distToThresh = threshold - MemStatsUnused(stats, d);
            printf("Domain %d has used %'.2fkb and is below threshold (%.1fkb)\n",
                d, -deltas->unused, threshold);
            // allocate double the amount needed to reach threshold since domain
            // is still eating up memory
            rt = AllocPlanAddAlloc(plan, d, ceil(2 * distToThresh));
            check(rt == 0, "failed to add allocation to plan");
        }
        else if (isUnusedBelowThreshold(stats, d)) {
            // domain is not using memory, but still starving
            // allocate only what's needed to reach threshold
            distToThresh = MIN_GUEST_MEMORY - MemStatsUnused(stats, d);
            printf("Domain %d is inactive but below threshold (%.1fkb)\n", d, threshold);
            rt = AllocPlanAddAlloc(plan, d, ceil(distToThresh));
        }
    }

    return 0;

error:
    return -1;
}

int deallocateWastefulGuests(AllocPlan *plan, MemStats *stats)
{
    int rt = 0;
    // DomainMemStats *deltas = NULL;
    MemStatUnit aboveThresh = 0;
    MemStatUnit toDealloc = 0;
    for (int d = 0; d < plan->numDomains; d++) {
        if (isWasteful(stats, d)) {
            aboveThresh = stats->domainStats[d].unused - MAX_FREE_MEMORY;
            toDealloc = aboveThresh / 2;
            printf("Domain %d is wasteful with %'.2fkb unused above threshold, to dealloc %'.2fkb\n",
                d, aboveThresh, toDealloc);
            rt = AllocPlanAddDealloc(plan, d, toDealloc);
            check(rt == 0, "failed to add wasteful dealloc to plan");
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
    unsigned long newSize;
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
    
    rt = AllocPlanComputeNewSizes(plan, stats, MIN_HOST_MEMORY);
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