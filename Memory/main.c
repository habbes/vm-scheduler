#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
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

    setlocale(LC_NUMERIC, "");

    conn = virConnectOpen(uri);
    check(conn, "Failed to connect to hypervisor");

    guests = GuestListGet(conn);
    check(conn, "Failed to create guest list");

    stats = MemStatsCreate(conn, guests);
    check(stats, "Failed to create memory stats\n");

    rt = MemStatsInit(stats, conn, guests);
    check(rt == 0, "failed to init memory stats");
    MemStatsPrint(stats);

    sleep(2);

    rt = MemStatsUpdate(stats, conn, guests, 1);
    check(rt == 0, "failed to update memory stats");
    MemStatsPrint(stats);

    rt = 0;
    goto final;

error:
    rt = 1;
final:
    cleanUp();
    return rt;
}
