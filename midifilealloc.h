#ifndef MIDIFILEALLOC_H
#define MIDIFILEALLOC_H

/* this is an internal header */
#include <stdlib.h>

/* pluggable allocators */
typedef struct __MfAllocators MfAllocators;
struct __MfAllocators {
    void *(*malloc)(size_t);
    const char *(*strerror)();
    void (*free)(void *);
};
extern MfAllocators Mf_Allocators;
#define AL Mf_Allocators

void *Mf_Malloc(size_t sz);
void *Mf_Calloc(size_t sz);

/* calloc of a type */
#define Mf_New(tp) (Mf_Calloc(sizeof(tp)))

#endif
