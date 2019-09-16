#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include "guestlist.h"
#include "memstats.h"
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

int main(int argc, char *argv[])
{
    char *uri = "qemu:///system";
    int rt = 0;

    conn = virConnectOpen(uri);
    check(conn, "Failed to connect to hypervisor");

    guests = GuestListGet(conn);
    check(conn, "Failed to create guest list");

    stats = MemStatsGet(conn, guests);
    check(stats, "Failed to get memory stats\n");

    MemStatsPrint(stats);

    rt = 0;
    goto final;

error:
    rt = 1;
final:
    cleanUp();
    return rt;
}