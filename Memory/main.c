#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include "guestlist.h"
#include "check.h"

virConnectPtr conn = NULL;
GuestList *guests = NULL;

void cleanUp()
{
    if (conn) {
        virConnectClose(conn);
    }
    if (guests) {
        GuestListFree(guests);
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

    printf("Guests found %d\n", guests->count);

    rt = 0;
    goto final;

error:
    rt = 1;
final:
    cleanUp();
    return rt;
}