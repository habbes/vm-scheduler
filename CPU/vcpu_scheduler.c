#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
    char * uri = "qemu:///system";
    virConnectPtr conn;
    int numDomains;
    int *activeDomains;
    int i = 0;

    conn = virConnectOpen(uri);
    if (conn == NULL) {
        fprintf(stderr, "Failed to connect to host %s\n", uri);
        goto error;
    }

    numDomains = virConnectNumOfDomains(conn);
    printf("Number of domains %d\n", numDomains);
    activeDomains = malloc(sizeof(int) * numDomains);
    if (!activeDomains) {
        fprintf(stderr, "Failed to allocated memory for active domains\n");
        goto error;
    }
    numDomains = virConnectListDomains(conn, activeDomains, numDomains);

    for (i = 0; i < numDomains; i++) {
        printf("Guest OS Id: %d\n", activeDomains[i]);
    }

    free(activeDomains);
    virConnectClose(conn);

    return 0;
error:
    if (conn) {
        virConnectClose(conn);
    }

    if (activeDomains) {
        free(activeDomains);
    }
    return 1;
}