#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "allocplan.h"

int AllocPlanAddAlloc(AllocPlan *plan, int domain, MemStatUnit size)
{
    checkNull(plan);
    check(domain >= 0 && domain < plan->numDomains, "invalid domain added to alloc plan");
    plan->toAlloc[domain] += size;

    return 0;
error:
    return -1;
}

int AllocPlanAddDealloc(AllocPlan *plan, int domain, MemStatUnit size)
{
    checkNull(plan);
    check(domain >= 0 && domain < plan->numDomains, "invalid domain added to alloc plan");
    plan->toDealloc[domain] += size;

    return 0;
error:
    return -1;
}

MemStatUnit AllocPlanTotalAlloc(AllocPlan *plan)
{
    MemStatUnit total = 0;
    for (int i = 0; i < plan->numDomains; i++) {
        total += plan->toAlloc[i];
    }
    return total;
}

MemStatUnit AllocPlanTotalDealloc(AllocPlan *plan)
{
    MemStatUnit total = 0;
    for (int i = 0; i < plan->numDomains; i++) {
        total += plan->toDealloc[i];
    }
    return total;
}

MemStatUnit AllocPlanDiff(AllocPlan *plan)
{
    return AllocPlanTotalAlloc(plan) - AllocPlanTotalDealloc(plan);
}

int AllocPlanComputeNewSizes(AllocPlan *plan, MemStats *stats)
{
    unsigned long newSize;
    checkNull(plan);
    checkNull(stats);

    for (int i = 0; i < plan->numDomains; i++) {
        newSize = (unsigned long) (stats->domainStats[i].actual + plan->toAlloc[i] - plan->toDealloc[i]);
        plan->newSizes[i] = newSize;
    }

    return 0;
error:
    return -1;
}

int AllocPlanReset(AllocPlan *plan)
{
    checkNull(plan);

    checkNull(memset(plan->toAlloc, 0, sizeof(MemStatUnit) * plan->numDomains));
    checkNull(memset(plan->toDealloc, 0, sizeof(MemStatUnit) * plan->numDomains));
    checkNull(memset(plan->newSizes, 0, sizeof(unsigned long) * plan->numDomains));

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
    plan->toAlloc = calloc(numDomains, sizeof(MemStatUnit));
    checkMemAlloc(plan->toAlloc);
    plan->toDealloc = calloc(numDomains, sizeof(MemStatUnit));
    checkMemAlloc(plan->toDealloc);
    plan->newSizes = calloc(numDomains, sizeof(unsigned long));
    checkMemAlloc(plan->newSizes);

    return plan;
error:
    free(plan);
    return NULL;
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
