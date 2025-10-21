#include "skred-mem.h"

#ifdef _WIN32

#include <stdio.h>

int skred_mem_create(skred_mem_t *sm, const char *name, size_t size) {
    sm->hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE,  // use the page file
        NULL,
        PAGE_READWRITE,
        0,
        (DWORD)size,
        name
    );
    if (!sm->hMap) return -1;

    sm->addr = MapViewOfFile(sm->hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!sm->addr) return -2;

    sm->size = size;
    return 0;
}

int skred_mem_open(skred_mem_t *sm, const char *name, size_t size) {
    sm->hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!sm->hMap) return -1;

    sm->addr = MapViewOfFile(sm->hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!sm->addr) return -2;

    sm->size = size;
    return 0;
}

void skred_mem_close(skred_mem_t *sm) {
    if (sm->addr) UnmapViewOfFile(sm->addr);
    if (sm->hMap) CloseHandle(sm->hMap);
}

#else

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int skred_mem_create(skred_mem_t *sm, const char *name, size_t size) {
    strncpy(sm->name, name, sizeof(sm->name));
    sm->fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (sm->fd < 0) return -1;
    if (ftruncate(sm->fd, size) != 0) return -2;

    sm->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, sm->fd, 0);
    if (sm->addr == MAP_FAILED) return -3;
    sm->size = size;
    return 0;
}

int skred_mem_open(skred_mem_t *sm, const char *name, size_t size) {
    strncpy(sm->name, name, sizeof(sm->name));
    sm->fd = shm_open(name, O_RDWR, 0666);
    if (sm->fd < 0) return -1;

    sm->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, sm->fd, 0);
    if (sm->addr == MAP_FAILED) return -2;
    sm->size = size;
    return 0;
}

void skred_mem_close(skred_mem_t *sm) {
    munmap(sm->addr, sm->size);
    close(sm->fd);
}

#endif
