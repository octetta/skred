#ifndef PORTABLE_WIN_H
#define PORTABLE_WIN_H

#ifdef _WIN32
#include <windows.h>
#include <time.h>

// 1. Threading types
typedef HANDLE pthread_t;
typedef int pthread_attr_t;

// 2. Clock stuff (MinGW/Zig sometimes miss these defines)
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

// 3. Dummy functions so the linker doesn't complain
static inline int pthread_attr_init(pthread_attr_t *a) { (void)a; return 0; }
static inline int pthread_attr_setstacksize(pthread_attr_t *a, size_t s) { (void)a; (void)s; return 0; }
static inline int pthread_detach(pthread_t t) { (void)t; return 0; }

// nanosleep wrapper for Windows
static inline int nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)rem;
    Sleep((DWORD)(req->tv_sec * 1000 + req->tv_nsec / 1000000));
    return 0;
}

// clock_gettime wrapper
static inline int clock_gettime(int clk_id, struct timespec *tp) {
    (void)clk_id;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned __int64 tim = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    tp->tv_sec = (long)((tim - 116444736000000000ULL) / 10000000ULL);
    tp->tv_nsec = (long)((tim % 10000000ULL) * 100);
    return 0;
}

// 4. pthread_create wrapper for Windows
static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    (void)attr;
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
    return (*thread == NULL);
}

#endif // _WIN32
#endif // PORTABLE_WIN_H
