#ifndef memstats_h
#define memstats_h

#include <libvirt/libvirt.h>
#include "guestlist.h"

#define MAX_STATS 15
typedef unsigned long long MemStatUnit;

typedef struct {
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
} DomainMemStats;

typedef struct {
    int numDomains;
    DomainMemStats *domainStats;
} MemStats;

MemStats *MemStatsGet(virConnectPtr conn, GuestList *guests);
void MemStatsFree(MemStats *stats);
void MemStatsPrint(MemStats *print);

#endif