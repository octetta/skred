#include "skred-mem.h"

#ifdef _WIN32
#include <stdio.h>
#include <string.h>

int skred_mem_create(skred_mem_t *sm, const char *name, size_t size) {
    // Use Global namespace for cross-session sharing
    char global_name[128];
    snprintf(global_name, sizeof(global_name), "Global\\%s", name);
    
    // Handle sizes > 4GB
    DWORD size_high = (DWORD)(size >> 32);
    DWORD size_low = (DWORD)(size & 0xFFFFFFFF);
    
    sm->hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        size_high,
        size_low,
        global_name
    );
    
    if (!sm->hMap) {
        sm->addr = NULL;
        sm->size = 0;
        return -1;
    }
    
    sm->addr = MapViewOfFile(sm->hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!sm->addr) {
        CloseHandle(sm->hMap);
        sm->hMap = NULL;
        sm->size = 0;
        return -2;
    }
    
    sm->size = size;
    return 0;
}

int skred_mem_open(skred_mem_t *sm, const char *name, size_t size) {
    // Use Global namespace for cross-session sharing
    char global_name[128];
    snprintf(global_name, sizeof(global_name), "Global\\%s", name);
    
    sm->hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, global_name);
    if (!sm->hMap) {
        sm->addr = NULL;
        sm->size = 0;
        return -1;
    }
    
    sm->addr = MapViewOfFile(sm->hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!sm->addr) {
        CloseHandle(sm->hMap);
        sm->hMap = NULL;
        sm->size = 0;
        return -2;
    }
    
    sm->size = size;
    return 0;
}

void skred_mem_close(skred_mem_t *sm) {
    if (sm->addr) {
        UnmapViewOfFile(sm->addr);
        sm->addr = NULL;
    }
    if (sm->hMap) {
        CloseHandle(sm->hMap);
        sm->hMap = NULL;
    }
    sm->size = 0;
}

#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int skred_mem_create(skred_mem_t *sm, const char *name, size_t size) {
    strncpy(sm->name, name, sizeof(sm->name)-1);
    sm->name[sizeof(sm->name)-1] = '\0';
    
    sm->fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (sm->fd < 0) {
        sm->addr = NULL;
        sm->size = 0;
        return -1;
    }
    
    if (ftruncate(sm->fd, size) != 0) {
        close(sm->fd);
        sm->fd = -1;
        sm->addr = NULL;
        sm->size = 0;
        return -2;
    }
    
    sm->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, sm->fd, 0);
    if (sm->addr == MAP_FAILED) {
        close(sm->fd);
        sm->fd = -1;
        sm->addr = NULL;
        sm->size = 0;
        return -3;
    }
    
    sm->size = size;
    return 0;
}

int skred_mem_open(skred_mem_t *sm, const char *name, size_t size) {
    strncpy(sm->name, name, sizeof(sm->name)-1);
    sm->name[sizeof(sm->name)-1] = '\0';
    
    sm->fd = shm_open(name, O_RDWR, 0666);
    if (sm->fd < 0) {
        sm->addr = NULL;
        sm->size = 0;
        return -1;
    }
    
    sm->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, sm->fd, 0);
    if (sm->addr == MAP_FAILED) {
        close(sm->fd);
        sm->fd = -1;
        sm->addr = NULL;
        sm->size = 0;
        return -2;
    }
    
    sm->size = size;
    return 0;
}

void skred_mem_close(skred_mem_t *sm) {
    if (sm->addr) {
        munmap(sm->addr, sm->size);
        sm->addr = NULL;
    }
    if (sm->fd >= 0) {
        close(sm->fd);
        sm->fd = -1;
    }
    sm->size = 0;
}

#endif