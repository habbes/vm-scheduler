#ifndef check_h
#define check_h

#include <stdio.h>

#define check(C, M) if (!(C)) {fprintf(stderr, (M)); fprintf(stderr, "\n"); goto error;}
#define checkMemAlloc(P) check(P, "failed to allocate memory")
#define checkNull(P) check(P, "null pointer")

#endif
