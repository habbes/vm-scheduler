#ifndef guestlist_h
#define guestlist_h

#include <libvirt/libvirt.h>

typedef struct Guest {
    int id;
    virDomainPtr domain;
} Guest;

typedef struct GuestList {
    int count;
    int *ids;
    virDomainPtr *domains;
} GuestList;


GuestList *GuestListGet(virConnectPtr conn);
void GuestListFree(GuestList *gl);
virDomainPtr GuestListDomainAt(GuestList *gl, int i);
int GuestListIdAt(GuestList *gl, int i);

#endif