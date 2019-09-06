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


void printParamValue(virTypedParameterPtr param)
{
    switch(param->type) {
        case VIR_TYPED_PARAM_INT:
            printf("%d", param->value.i);
            break;
        case VIR_TYPED_PARAM_LLONG:
            printf("%lld", param->value.l);
            break;
        case VIR_TYPED_PARAM_ULLONG:
            printf("%llu", param->value.l);
            break;
        case VIR_TYPED_PARAM_STRING:
            printf("%s", param->value.s);
            break;
        case VIR_TYPED_PARAM_DOUBLE:
            printf("%.2f", param->value.d);
            break;
        default:
            printf("%llu", param->value);
    }
}

int main(int argc, char *argv[])
{
    char * uri = "qemu:///system";
    virConnectPtr conn = NULL;
    virNodeInfo nodeInfo;
    GuestList *guestList = NULL;
    virDomainInfo domainInfo;
    virDomainPtr domain;
    virTypedParameterPtr params;
    int i = 0;

    conn = virConnectOpen(uri);
    check(conn, "Failed to connect to host");

    guestList = guestListGet(conn);
    check(guestList, "Failed to create guest list");

    for (i = 0; i < guestList->count; i++) {
        domain = guestListDomainAt(guestList, i);
        virDomainGetInfo(domain, &domainInfo);
        printf("Guest VM Id: %d\n", guestListIdAt(guestList, i));
        printf("- state: %u\n", domainInfo.state);
        printf("- max mem: %lu\n", domainInfo.maxMem);
        printf("- mem: %lu\n", domainInfo.memory);
        printf("- num vcpus: %u\n", domainInfo.nrVirtCpu);
        printf("- cpu time: %lld\n", domainInfo.cpuTime);

        int ncpus = virDomainGetCPUStats(domain, NULL, 0, 0, 0, 0);
        int nparams = virDomainGetCPUStats(domain, NULL, 0, 0, 1, 0);
        params = calloc(ncpus * nparams, sizeof(virTypedParameter));
        virDomainGetCPUStats(domain, params, nparams, 0, ncpus, 0);
        for (int c = 0; c < ncpus; c++) {
            printf("-- cpu %d\n", c);
            for (int p = 0; p < nparams; p++) {
                printf("--- param %s type %d value ", params[p].field, params[p].type);
                printParamValue(params + (nparams * c + p));
                printf("\n");
            }
        }
        puts("");
        free(params);
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