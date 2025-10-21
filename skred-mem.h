#ifndef _SKRED_SHM_H_
#define _SKRED_SHM_H_

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <stddef.h>

typedef struct skred_mem_t {
    void *addr;
    size_t size;
#ifdef _WIN32
    HANDLE hMap;
#else
    int fd;
    char name[64];
#endif
} skred_mem_t;

int skred_mem_create(skred_mem_t *sm, const char *name, size_t size);
int skred_mem_open(skred_mem_t *sm, const char *name, size_t size);
void skred_mem_close(skred_mem_t *sm);

#endif
