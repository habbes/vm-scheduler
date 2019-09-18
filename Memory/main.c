#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <signal.h>
#include <libvirt/libvirt.h>
#include "guestlist.h"
#include "memstats.h"
#include "coordinator.h"
#include "check.h"

virConnectPtr conn = NULL;
GuestList *guests = NULL;
MemStats *stats = NULL;


void cleanUp()
{
    if (conn) {
        virConnectClose(conn);
    }
    if (guests) {
        GuestListFree(guests);
    }
    if (stats) {
        MemStatsFree(stats);
    }
}

void sigintHandler(int sigNum)
{
    printf("Terminating due to keyboard interrupt...");
    cleanUp();
    exit(0);
}

int main(int argc, char *argv[])
{
    char *uri = "qemu:///system";
    int rt = 0;
    int interval = 0;

    signal(SIGINT, sigintHandler);

    check(argc > 1, "interval arg required, usage: ./cpu_scheduler <interval>");
    interval = atoi(argv[1]);

    setlocale(LC_NUMERIC, "");

    conn = virConnectOpen(uri);
    check(conn, "Failed to connect to hypervisor");

    guests = GuestListGet(conn);
    check(conn, "Failed to create guest list");

    for(int i = 0; i < guests->count; i++) {
        rt = virDomainSetMemoryStatsPeriod(GuestListDomainAt(guests, i), 1, 0);
        check(rt == 0, "failed to set memory stats period");
    }

    stats = MemStatsCreate(conn, guests);
    check(stats, "Failed to create memory stats\n");

    rt = MemStatsInit(stats, conn, guests);
    check(rt == 0, "failed to init memory stats");
    MemStatsPrint(stats);

    sleep(2);

    rt = MemStatsUpdate(stats, conn, guests, 1);
    check(rt == 0, "failed to update memory stats");
    MemStatsPrint(stats);


    while (1) {
        puts("sleeping...");
        sleep(interval);
        puts("coordinating...");
        rt = MemStatsUpdate(stats, conn, guests, 1);
        check(rt == 0, "error updating stats");
        MemStatsPrint(stats);
        rt = reallocateMemory(stats, guests);
        check(rt == 0, "error re-allocating memory");
        // update stats to match the new allocations
        rt = MemStatsUpdate(stats, conn, guests, 0);
        check(rt == 0, "error updating stats");
        puts("memory coordination cycle done\n");
    }

    rt = 0;
    goto final;

error:
    rt = 1;
final:
    cleanUp();
    return rt;
}
