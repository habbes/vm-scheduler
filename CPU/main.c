#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libvirt/libvirt.h>
#include "check.h"
#include "guestlist.h"
#include "cpustats.h"
#include "scheduler.h"
#include "util.h"



int main(int argc, char *argv[])
{
    int interval = 0;
    char * uri = "qemu:///system";
    virConnectPtr conn = NULL;
    GuestList *guests = NULL;
    CpuStats *stats = NULL;
    int rt = 0;

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

    sleep(2);
    rt = updateStats(stats, guests, 2);
    check(rt == 0, "error updating stats");
    CpuStatsPrint(stats);

    rt = allocateCpus(conn, stats, guests);
    check(rt == 0, "error allocating cpus");

    while (1) {
        puts("sleeping...");
        sleep(interval);
        puts("scheduling...");
        rt = updateStats(stats, guests, interval);
        check(rt == 0, "error updating stats");
        rt = allocateCpus(conn, stats, guests);
        check(rt == 0, "error allocating cpus");
        puts("scheduling cycle done\n");
    }

    // unsigned char cpumap = 0x0f;
    // for (int d = 0; d < guests->count; d++) {
    //     virDomainPinVcpu(GuestListDomainAt(guests, d), 0, &cpumap, 1);
    // }

    GuestListFree(guests);
    CpuStatsFree(stats);
    virConnectClose(conn);

    return 0;
error:
    if (guests) {
        GuestListFree(guests);
    }
    if (conn) {
        virConnectClose(conn);
    }
    if (stats) {
        CpuStatsFree(stats);
    }
    return 1;
}