#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "allocplan.h"

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
