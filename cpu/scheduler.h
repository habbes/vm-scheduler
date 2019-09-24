#ifndef scheduler_h
#define scheduler_h

#include "cpustats.h"
#include "guestlist.h"

int repinCpus(CpuStats *stats, GuestList *guests, CpuStatsUsage_t *targetWeights);
int allocateCpus(CpuStats *stats, GuestList *guests);

#endif