#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

#define check(C, M) if(!(C)){fprintf(stderr, (M)); fprintf(stderr, "\n"); goto error;}

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
    printf("Domains ptr %p num domains %d size each %ld\n", guestList->domains, numDomains, sizeof(virDomainPtr));
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

int main(int argc, char *argv[])
{
    char * uri = "qemu:///system";
    virConnectPtr conn = NULL;
    GuestList *guestList = NULL;
    virDomainInfo domainInfo;
    int i = 0;

    conn = virConnectOpen(uri);
    check(conn, "Failed to connect to host");

    guestList = guestListGet(conn);
    check(guestList, "Failed to create guest list");

    for (i = 0; i < guestList->count; i++) {
        virDomainGetInfo(guestListDomainAt(guestList, i), &domainInfo);
        printf("Guest OS Id: %d\n", guestListIdAt(guestList, i));
        printf("- state: %u\n", domainInfo.state);
        printf("- max mem: %lu\n", domainInfo.maxMem);
        printf("- mem: %lu\n", domainInfo.memory);
        printf("- num vcpus: %u\n", domainInfo.nrVirtCpu);
        printf("- cpu tume: %lld\n", domainInfo.cpuTime);
        puts("");
    }

    guestListFree(guestList);
    virConnectClose(conn);

    return 0;
error:
    if (guestList) {
        guestListFree(guestList);
    }
    if (conn) {
        virConnectClose(conn);
    }
    return 1;
}