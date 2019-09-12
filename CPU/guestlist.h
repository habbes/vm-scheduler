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


GuestList *guestListGet(virConnectPtr conn);
void guestListFree(GuestList *gl);
virDomainPtr guestListDomainAt(GuestList *gl, int i);
int guestListIdAt(GuestList *gl, int i);

#endif