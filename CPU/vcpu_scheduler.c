#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libvirt/libvirt.h>

#define check(C, M) if (!(C)) {fprintf(stderr, (M)); fprintf(stderr, "\n"); goto error;}
#define checkMemAlloc(P) check(P, "failed to allocate memory")

/* -- guest -- */
typedef struct Guest {
    int id;
    virDomainPtr domain;
} Guest;

typedef struct GuestList {
    int count;
    int *ids;
    virDomainPtr *domains;
} GuestList;

GuestList *guestListGet(virConnectPtr conn)
{
    int i = 0;
    int numDomains = 0;
    GuestList *guestList = malloc(sizeof(GuestList));
    check(guestList, "failed to allocated guest list.");
    guestList->count = 0;

    numDomains = virConnectNumOfDomains(conn);
    guestList->ids = calloc(numDomains, sizeof(int));
    check(guestList->ids, "failed to allocated domain ids");
    
    numDomains = virConnectListDomains(conn, guestList->ids, numDomains);
    check(numDomains >= 0, "Failed to list domains");
    guestList->count = numDomains;

    guestList->domains = malloc(numDomains * sizeof(virDomainPtr));
    check(guestList->domains, "failed to allocated guest domains");

    for (i = 0; i < numDomains; i++) {
        guestList->domains[i] = virDomainLookupByID(conn, guestList->ids[i]);
    }

    return guestList;
error:
    if (guestList) {
        if (guestList->domains) {
            free(guestList->domains);
        }
        if (guestList->ids) {
            free(guestList->ids);
        }
        free(guestList);
    }
    return NULL;
}

void guestListFree(GuestList *gl) {
    int i =  0;
    if (!gl) {
        return;
    }
    if (gl->domains) {
        for (i = 0; i < gl->count; i++) {
            virDomainFree(gl->domains[i]);
        }
        free(gl->domains);
    }
    if (gl->ids) {
        free(gl->ids);
    }
    free(gl);
}

virDomainPtr guestListDomainAt(GuestList *gl, int i)
{
    return gl->domains[i];
}

int guestListIdAt(GuestList *gl, int i)
{
    return gl->ids[i];
}

/* -- cpu stats -- */

typedef long double CpuStatsUsage_t;

typedef struct CpuStats {
    int numCpus;
    int numDomains;
    CpuStatsUsage_t *usages;
    unsigned long long *times;
} CpuStats;

#define CpuStatsCheckStatsArg(stats) check(stats, "stats is null")
#define CpuStatsCheckCpuArg(cpu) check(cpu >= 0 && cpu < stats->numCpus, "cpu out of bounds")
#define CpuStatsCheckDomainArg(domain) check(domain >= 0 && domain < stats->numDomains, "domain out of bounds")

#define CpuStatsCheckArgs(stats, cpu, domain) CpuStatsCheckStatsArg(stats);\
    CpuStatsCheckCpuArg(cpu);\
    CpuStatsCheckDomainArg(domain);

CpuStats *CpuStatsCreate(int cpus, int domains)
{
    CpuStats *stats = calloc(1, sizeof(CpuStats));
    checkMemAlloc(stats);
    stats->numCpus = cpus;
    stats->numDomains = domains;
    stats->usages = calloc(cpus, sizeof(CpuStatsUsage_t));
    checkMemAlloc(stats->usages);
    stats->times = calloc(domains * cpus, sizeof(unsigned long long));
    checkMemAlloc(stats->times);

    return stats;

error:
    if (stats) {
        if (stats->usages) {
            free(stats->usages);
        }
        if (stats->times) {
            free(stats->times);
        }
        free(stats);
    }
    return NULL;
}

void CpuStatsFree(CpuStats *stats)
{
    if (stats) {
        if (stats->usages) {
            free(stats->usages);
        }
        if (stats->times) {
            free(stats->times);
        }
        free(stats);
    }
}

int CpuStatsSetTime(CpuStats *stats, int cpu, int domain, unsigned long long time)
{
    CpuStatsCheckArgs(stats, cpu, domain);
    *(stats->times + stats->numCpus * domain + cpu) = time;
    return 0;
error:
    return -1;
}

int CpuStatsGetTime(CpuStats *stats, int cpu, int domain, unsigned long long *timePtr)
{
    CpuStatsCheckArgs(stats, cpu, domain);
    *timePtr = *(stats->times + stats->numCpus * domain + cpu);
    return 0;
error:
    return -1;
}

int CpuStatsResetUsages(CpuStats *stats)
{
    check(stats, "stats is null");
    memset(stats->usages, 0, sizeof(CpuStatsUsage_t) * stats->numCpus);
    return 0;
error:
    return 1;
}

int CpuStatsAddUsage(CpuStats *stats, int cpu, CpuStatsUsage_t usage)
{
    CpuStatsCheckStatsArg(stats);
    CpuStatsCheckCpuArg(cpu);

    stats->usages[cpu] = stats->usages[cpu] + usage;
    return 0;
error:
    return -1;
}

int CpuStatsUsagesToPct(CpuStats *stats, double timeInterval)
{
    CpuStatsCheckStatsArg(stats);

    for (int i = 0; i < stats->numCpus; i++) {
        stats->usages[i] = stats->usages[i] / 1e9;
        if (timeInterval > 0) {
            stats->usages[i] = stats->usages[i] / timeInterval;
        }
    }
    return 0;

error:
    return -1;
}

int CpuStatsPrint(CpuStats *stats)
{
    unsigned long long cpuTime = 0;
    check(stats, "Stats cannot be null");

    for (int c = 0; c < stats->numCpus; c++) {
        printf("cpu %d usage: %.2Lf\n", c, 100 * stats->usages[c]);
    }

    for (int i = 0; i < stats->numDomains; i++) {
        printf("domain %d\n", i);
        for (int c = 0; c < stats->numCpus; c++) {
            cpuTime = *(stats->times + stats->numCpus * i + c);
            printf("-- cpuTime %d: %llu\n", c, cpuTime);
        }
    }

    return 0;
error:
    return -1;
}

int updateStats(CpuStats *stats, GuestList *guests, int timeInterval)
{
    int nparams = 0;
    int d = 0; // domain iterator
    int c = 0; // cpu iterator
    int p = 0; // param iterator
    int paramPos = 0;
    unsigned long long prevTime = 0L;
    unsigned long long currTime = 0L;
    unsigned long long timeDiff = 0L;
    int rt = 0;
    virDomainPtr domain = NULL;
    virTypedParameterPtr params = NULL;

    check(stats, "stats is null");
    check(guests, "guests is null");
 
    nparams = virDomainGetCPUStats(guestListDomainAt(guests, 0), NULL, 0, 0, 1, 0);
    check(nparams >= 0, "failed to get domain cpu params");
    params = calloc(stats->numCpus * nparams, sizeof(virTypedParameter));
    check(params, "failed to allocated params");

    rt = CpuStatsResetUsages(stats);
    check(rt == 0, "failed to reset usages");

    for (d = 0; d < guests->count; d++) {
        domain = guestListDomainAt(guests, d);
        virDomainGetCPUStats(domain, params, nparams, 0, stats->numCpus, 0);

        for (c = 0; c < stats->numCpus; c++) {
            for (p = 0; p < nparams; p++) {
                paramPos = nparams * c + p;

                if (strncmp(params[paramPos].field, "vcpu_time", VIR_TYPED_PARAM_FIELD_LENGTH) == 0) {
                    rt = CpuStatsGetTime(stats, c, d, &prevTime);
                    check(rt == 0, "failed to get cpu time from stats");
                    currTime = params[paramPos].value.ul;
                    timeDiff = prevTime > 0 ? currTime - prevTime : 0;
                    printf("--- cpu %d, domain %d, cur time %llu prev time %llu diff %llu\n",
                        c, d, currTime, prevTime, timeDiff);
                    rt = CpuStatsAddUsage(stats, c, (CpuStatsUsage_t) timeDiff);
                    check(rt == 0, "failed to add cpu usage");
                    rt = CpuStatsSetTime(stats, c, d, currTime);
                    check(rt == 0, "failed to updated cpu time");
                }
            }
        }
        puts("");
    }

    rt = CpuStatsUsagesToPct(stats, timeInterval);
    check(rt == 0, "failed to updated usages");

    rt = 0;
    goto final;

error:
    rt = -1;
final:
    if (params) {
        free(params);
    }
    return rt;
}


void printParamValue(virTypedParameterPtr param)
{
    switch(param->type) {
        case VIR_TYPED_PARAM_INT:
            printf("%d", param->value.i);
            break;
        case VIR_TYPED_PARAM_LLONG:
            printf("%lld", param->value.l);
            break;
        case VIR_TYPED_PARAM_ULLONG:
            printf("%llu", param->value.l);
            break;
        case VIR_TYPED_PARAM_STRING:
            printf("%s", param->value.s);
            break;
        case VIR_TYPED_PARAM_DOUBLE:
            printf("%.2f", param->value.d);
            break;
        default:
            printf("%llu", param->value.ul);
    }
}

int main(int argc, char *argv[])
{
    char * uri = "qemu:///system";
    virConnectPtr conn = NULL;
    GuestList *guests = NULL;
    CpuStats *stats = NULL;

    conn = virConnectOpen(uri);
    check(conn, "Failed to connect to host");

    guests = guestListGet(conn);
    check(guests, "Failed to create guest list");

    stats = CpuStatsCreate(4, guests->count);
    check(stats, "Failed to create stats");

    updateStats(stats, guests, -1);
    CpuStatsPrint(stats);

    // unsigned char cpumap = 0x0f;
    // for (int d = 0; d < guests->count; d++) {
    //     virDomainPinVcpu(guestListDomainAt(guests, d), 0, &cpumap, 1);
    // }

    sleep(2);
    updateStats(stats, guests, 2);
    CpuStatsPrint(stats);

    guestListFree(guests);
    CpuStatsFree(stats);
    virConnectClose(conn);

    return 0;
error:
    if (guests) {
        guestListFree(guests);
    }
    if (conn) {
        virConnectClose(conn);
    }
    if (stats) {
        free(stats);
    }
    return 1;
}