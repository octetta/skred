/* src/nsync_local.c */
#include <stdint.h>

#define NSYNC_S_IN_MS (1000)
#define NSYNC_MS_IN_US (1000)
#define NSYNC_US_IN_NS (1000)
#define NSYNC_S_IN_NS (NSYNC_S_IN_MS * NSYNC_MS_IN_US * NSYNC_US_IN_NS)

#if defined(_WIN32)
    #define NSYNC_USE_WIN32
    #include "nsync_local/platform/win32/per_thread_waiter.c"
    #include "nsync_local/platform/win32/sem.c"
    #include "nsync_local/platform/win32/time_rep.c"
#else
    #define NSYNC_USE_PTHREADS
    #include "nsync_local/platform/posix/per_thread_waiter.c"
    #include "nsync_local/platform/posix/yield.c"
    #include "nsync_local/platform/posix/nsync_panic.c"
    #include "nsync_local/platform/posix/time_rep.c"
#if defined(__APPLE__)
    #include "nsync_local/platform/posix/src/sem_mutex.c"
#else
    #include "nsync_local/platform/posix/sem.c"
#endif
#endif

#include "nsync_local/internal/common.c"
#include "nsync_local/internal/dll.c"
#include "nsync_local/internal/mu.c"
#include "nsync_local/internal/wait.c"
#include "nsync_local/internal/time_internal.c"
