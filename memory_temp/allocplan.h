#ifndef allocplan_h
#define allocplan_h

#include "memstats.h"

typedef struct AllocPlan {
    int numDomains;
    MemStatUnit *toAlloc;
    MemStatUnit *toDealloc;
    unsigned long *newSizes;
} AllocPlan;

#define AllocPlanGetNewSize(plan, domain) ((plan)->newSizes[(domain)])

AllocPlan *AllocPlanCreate(int numDomains);
void AllocPlanFree(AllocPlan *plan);
int AllocPlanReset(AllocPlan *plan);
MemStatUnit AllocPlanDiff(AllocPlan *plan);
int AllocPlanAddAlloc(AllocPlan *plan, int domain, MemStatUnit size);
int AllocPlanAddDealloc(AllocPlan *plan, int domain, MemStatUnit size);
int AllocPlanComputeNewSizes(AllocPlan *plan, MemStats *stats);

#endif
