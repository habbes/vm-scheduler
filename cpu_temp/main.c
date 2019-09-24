#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <libvirt/libvirt.h>
#include "check.h"
#include "guestlist.h"
#include "cpustats.h"
#include "scheduler.h"
#include "util.h"

virConnectPtr conn = NULL;
GuestList *guests = NULL;
CpuStats *stats = NULL;

void cleanUp()
{
    if (conn) {
        virConnectClose(conn);
    }
    if (guests) {
        GuestListFree(guests);
    }
    if (stats) {
        CpuStatsFree(stats);
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
    int interval = 0;
    char * uri = "qemu:///system";
    virConnectPtr conn = NULL;
    GuestList *guests = NULL;
    CpuStats *stats = NULL;
    int rt = 0;

    signal(SIGINT, sigintHandler);

    check(argc > 1, "interval arg required, usage: ./cpu_scheduler <interval>");
    interval = atoi(argv[1]);

    conn = virConnectOpen(uri);
    check(conn, "Failed to connect to host");

    guests = GuestListGet(conn);
    check(guests, "Failed to create guest list");

    stats = CpuStatsCreate(4, guests->count);
    check(stats, "Failed to create stats");

    rt = updateStats(stats, guests, -1);
    check(rt == 0, "error updating stats");
    CpuStatsPrint(stats);

    while (1) {
        puts("sleeping...");
        sleep(interval);
        puts("scheduling...");
        rt = updateStats(stats, guests, interval);
        check(rt == 0, "error updating stats");
        rt = allocateCpus(stats, guests);
        check(rt == 0, "error allocating cpus");
        puts("scheduling cycle done\n");
    }

    rt = 0;
    goto final;

error:
    rt = 1;
final:
    cleanUp();
    return rt;
}