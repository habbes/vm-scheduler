#ifndef scheduler_h
#define scheduler_h

#include "cpustats.h"
#include "guestlist.h"

int getDomainToUnpinFromCpu(unsigned char *cpuMaps, int numDomains, unsigned char cpumask, int numCpus);
int getDomainToPinToCpu(unsigned char *cpuMaps, int numDomains, unsigned char cpumask, int numCpus);
int repinCpus(virConnectPtr conn, CpuStats *stats, GuestList *guests, int *targetDiffs);
int allocateCpus(virConnectPtr conn, CpuStats *stats, GuestList *guests);

#endif