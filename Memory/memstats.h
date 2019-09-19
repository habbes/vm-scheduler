#ifndef memstats_h
#define memstats_h

#include <libvirt/libvirt.h>
#include "guestlist.h"

#define MAX_STATS 15
typedef double MemStatUnit;

typedef struct DomainMemStats {
    /**
     * Current balloon value (in KB).
     */
    MemStatUnit actual;
    /**
     * The amount of memory left completely unused by the system.
     * Memory that is available but used for reclaimable caches should NOT be reported as free.
     * This value is expressed in kB.
     */
    MemStatUnit unused;
    /**
     * How much the balloon can be inflated without pushing the guest system to swap,
     * corresponds to 'Available' in /proc/meminfo
     */
    MemStatUnit usable;
    /**
     * The total amount of usable memory as seen by the domain.
     * This value may be less than the amount of memory assigned to the domain
     * if a balloon driver is in use or if the guest OS does not initialize all assigned pages.
     * This value is expressed in kB.
     */
    MemStatUnit available;
    /**
     * Maximum amount of physical memory allocated to the domain
     */
    MemStatUnit max;
} DomainMemStats;

typedef struct HostMemStats {
    MemStatUnit total;
    MemStatUnit free;
} HostMemStats;

typedef struct MemStats {
    int numDomains;
    DomainMemStats *domainStats;
    HostMemStats hostStats;
    DomainMemStats *domainDeltas;
} MemStats;

#define MemStatsUnused(stats, dom) ((stats)->domainStats[(dom)].unused)
MemStats *MemStatsCreate(virConnectPtr conn, GuestList *guests);
void MemStatsFree(MemStats *stats);
void MemStatsPrint(MemStats *print);
int MemStatsInit(MemStats *stats, virConnectPtr conn, GuestList *guests);
int MemStatsUpdate(MemStats *stats, virConnectPtr conn, GuestList *guests, int updateDeltas);

#endif