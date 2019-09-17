#ifndef allocplan_h
#define allocplan_h

#include "memstats.h"

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

AllocPlan *AllocPlanCreate(int numDomains);
void AllocPlanFree(AllocPlan *plan);
int AllocPlanReset(AllocPlan *plan);
MemStatUnit AllocPlanDiff(AllocPlan *plan);
int AllocPlanAddAlloc(AllocPlan *plan, int domain, MemStatUnit size);
int AllocPlanAddDealloc(AllocPlan *plan, int domain, MemStatUnit size);

#endif
