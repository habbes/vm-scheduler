#include <stdlib.h>
#include <libvirt/libvirt.h>
#include "check.h"
#include "guestlist.h"

GuestList *GuestListGet(virConnectPtr conn)
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

void GuestListFree(GuestList *gl) {
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

virDomainPtr GuestListDomainAt(GuestList *gl, int i)
{
    return gl->domains[i];
}

int GuestListIdAt(GuestList *gl, int i)
{
    return gl->ids[i];
}