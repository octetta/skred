/* src/nsync_local.c */
#include "nsync_local/public/nsync.h"  // <--- Ensure this is FIRST
#include <stdint.h>

#define NSYNC_S_IN_MS (1000)
#define NSYNC_MS_IN_US (1000)
#define NSYNC_US_IN_NS (1000)
#define NSYNC_S_IN_NS (NSYNC_S_IN_MS * NSYNC_MS_IN_US * NSYNC_US_IN_NS)

#if defined(_WIN32)
    #define NSYNC_USE_WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <time.h> // Ensure time_t is defined

    /* Match the declaration in nsync_time.h exactly */
    nsync_time nsync_time_s_ns(time_t s, unsigned int ns) {
        nsync_time t; 
        t.tv_sec = s; 
        t.tv_nsec = ns; 
        return t;
    }

    int nsync_time_cmp(nsync_time a, nsync_time b) {
        if (a.tv_sec != b.tv_sec) return (a.tv_sec > b.tv_sec) ? 1 : -1;
        if (a.tv_nsec != b.tv_nsec) return (a.tv_nsec > b.tv_nsec) ? 1 : -1;
        return 0;
    }

    /* Windows nsync_time (struct timespec) uses tv_sec and tv_nsec fields */
    const nsync_time nsync_time_zero = {0, 0};
    const nsync_time nsync_time_no_deadline = {((time_t)1 << 62), 0};

    #include "nsync_local/platform/win32/per_thread_waiter.c"
    #include "nsync_local/platform/win32/sem.c"
    #include "nsync_local/platform/win32/time_rep.c"
    #include "nsync_local/platform/generic/nsync_panic.c"
#else
    #define NSYNC_USE_PTHREADS
    #include "nsync_local/platform/posix/per_thread_waiter.c"
    #include "nsync_local/platform/posix/yield.c"
    #include "nsync_local/platform/posix/nsync_panic.c"
    #include "nsync_local/platform/posix/time_rep.c"
#if defined(__APPLE__)
    #include "nsync_local/platform/posix/sem_mutex.c"
#else
    #include "nsync_local/platform/posix/sem.c"
#endif
#endif

#include "nsync_local/internal/common.c"
#include "nsync_local/internal/dll.c"
#include "nsync_local/internal/mu.c"
#include "nsync_local/internal/wait.c"
#include "nsync_local/internal/time_internal.c"
