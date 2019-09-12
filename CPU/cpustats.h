#ifndef cpustats_h
#define cpustats_h

#include "check.h"

typedef long double CpuStatsUsage_t;
typedef unsigned long long CpuStatsTime_t;
typedef long double CpuStatsWeight_t;

typedef struct CpuStats {
    int numCpus;
    int numDomains;
    CpuStatsUsage_t *usages;
    CpuStatsUsage_t *domainUsages;
    CpuStatsWeight_t *cpuWeights;
    CpuStatsTime_t *times;
} CpuStats;

#define CpuStatsCheckStatsArg(stats) check(stats, "stats is null")
#define CpuStatsCheckCpuArg(cpu) check(cpu >= 0 && cpu < stats->numCpus, "cpu out of bounds")
#define CpuStatsCheckDomainArg(domain) check(domain >= 0 && domain < stats->numDomains, "domain out of bounds")

#define CpuStatsCheckArgs(stats, cpu, domain) CpuStatsCheckStatsArg(stats);\
    CpuStatsCheckCpuArg(cpu);\
    CpuStatsCheckDomainArg(domain);

#define CpuStatsGetUsage(stats, cpu) ((stats)->usages[(cpu)])
#define CpuStatsGetCpuWeight(stats, cpu) ((stats)->cpuWeights[(cpu)])

/**
 * creates cpu stats object
 * @param cpus number of cpus
 * @param domains number of domains
 * @return pointer to stats object. Created object should be freed using CpuStatsFree()
 */
CpuStats *CpuStatsCreate(int cpus, int domains);
void CpuStatsFree(CpuStats *);
int CpuStatsSetTime(CpuStats *stats, int cpu, int domain, CpuStatsTime_t time);
int CpuStatsGetTime(CpuStats *stats, int cpu, int domain, CpuStatsTime_t *timePtr);
int CpuStatsResetUsages(CpuStats *stats);
int CpuStatsAddUsage(CpuStats *stats, int cpu, CpuStatsUsage_t usage);
int CpuStatsAddDomainUsage(CpuStats *stats, int domain, CpuStatsUsage_t usage);
int CpuStatsUsagesToPct(CpuStats *stats, double timeInterval);
int CpuStatsPrint(CpuStats *stats);

#endif
