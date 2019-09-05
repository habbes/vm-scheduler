#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
    char * uri = "qemu:///system";
    virConnectPtr conn;
    char *hostname;

    conn = virConnectOpen(uri);
    if (conn == NULL) {
        fprintf(stderr, "Failed to connect to host %s\n", uri);
        return 1;
    }

    hostname = virConnectGetHostname(conn);
    printf("Connected to host %s\n", hostname);
    free(hostname);

    virConnectClose(conn);

    return 0;
}