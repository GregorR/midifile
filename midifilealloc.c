#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "midifilealloc.h"

MfAllocators Mf_Allocators;
#define AL Mf_Allocators

/* internal malloc-wrapper with error checking */
void *Mf_Malloc(size_t sz)
{
    void *ret = AL.malloc(sz);
    if (ret == NULL) {
        fprintf(stderr, "Error while allocating memory: %s\n", AL.strerror());
        exit(1);
    }
    return ret;
}

/* same for calloc */
void *Mf_Calloc(size_t sz)
{
    void *ret = Mf_Malloc(sz);
    memset(ret, 0, sz);
    return ret;
}
