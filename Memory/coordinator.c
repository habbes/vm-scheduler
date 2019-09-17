#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "coordinator.h"

#define LOW_UNUSED_THRESHOLD 0.2
#define SAFE_UNUSED_THRESHOLD 0.3

#define unusedPct(stats, dom) ((stats)->domainStats[(dom)].unused / (stats)->domainStats[(dom)].actual)

#define isUnusedBelowThreshold(stats, dom) (unusedPct(stats, dom) < LOW_UNUSED_THRESHOLD)

#define canDeallocate(stats, dom) (unusedPct(stats, dom) > SAFE_UNUSED_THRESHOLD)

typedef struct DomainAlloc {
    int domain;
    MemStatUnit size;
} DomainAlloc;

typedef struct AllocPlan {
    int numAlloc;
    int numDealloc;
    int numDomains;
    DomainAlloc *toAlloc;
    DomainAlloc *toDealloc;
} AllocPlan;

int AllocPlanAddAlloc(AllocPlan *plan, int domain, MemStatUnit size)
{
    checkNull(plan);
    check(domain >= 0 && domain < plan->numDomains, "invalid domain added to alloc plan");
    check(plan->numAlloc < plan->numDomains, "domains to allocate full");
    DomainAlloc *alloc = plan->toAlloc + plan->numAlloc;

    alloc->domain = domain;
    alloc->size = size;
    plan->numAlloc++;

    return 0;
error:
    return -1;
}

int AllocPlanAddDealloc(AllocPlan *plan, int domain, MemStatUnit size)
{
    checkNull(plan);
    check(domain >= 0 && domain < plan->numDomains, "invalid domain added to alloc plan");
    check(plan->numDealloc < plan->numDomains, "domains to deallocate full");
    DomainAlloc *dealloc = plan->toDealloc + plan->numDealloc;

    dealloc->domain = domain;
    dealloc->size = size;
    plan->numDealloc++;

    return 0;
error:
    return -1;
}

AllocPlan *AllocPlanCreate(int numDomains)
{
    AllocPlan *plan = NULL;
    plan = calloc(1, sizeof(AllocPlan));
    checkMemAlloc(plan);
    plan->numDomains = numDomains;
    plan->toAlloc = calloc(numDomains, sizeof(DomainAlloc));
    checkMemAlloc(plan->toAlloc);
    plan->toDealloc = calloc(numDomains, sizeof(DomainAlloc));
    checkMemAlloc(plan->toDealloc);

    return plan;
error:
    free(plan);
    return NULL;
}

MemStatUnit AllocPlanTotalAlloc(AllocPlan *plan)
{
    MemStatUnit total = 0;
    for (int i = 0; i < plan->numAlloc; i++) {
        total += plan->toAlloc[i].size;
    }
    return total;
}

MemStatUnit AllocPlanTotalDealloc(AllocPlan *plan)
{
    MemStatUnit total = 0;
    for (int i = 0; i < plan->numDealloc; i++) {
        total += plan->toDealloc[i].size;
    }
    return total;
}

MemStatUnit AllocPlanDiff(AllocPlan *plan)
{
    return AllocPlanTotalAlloc(plan) - AllocPlanTotalDealloc(plan);
}

void AllocPlanFree(AllocPlan *plan)
{
    if (plan) {
        if (plan->toAlloc) {
            free(plan->toAlloc);
        }
        if (plan->toDealloc) {
            free(plan->toDealloc);
        }
        free(plan);
    }
}

int AllocPlanReset(AllocPlan *plan)
{
    checkNull(plan);

    plan->numAlloc = 0;
    plan->numDealloc = 0;
    checkNull(memset(plan->toAlloc, 0, sizeof(DomainAlloc)));
    checkNull(memset(plan->toDealloc, 0, sizeof(DomainAlloc)));

    return 0;

error:
    return -1;
}

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