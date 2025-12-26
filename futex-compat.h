// futex-compat.h - Cross-platform futex-like implementation
#ifndef FUTEX_COMPAT_H
#define FUTEX_COMPAT_H

#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Windows 8+ has WaitOnAddress which is exactly like futex
static inline long futex_wait_timeout(volatile uint32_t *uaddr, uint32_t expected, int timeout_ms) {
    // WaitOnAddress is available on Windows 8+
    // For older Windows, you'd need to use Events or critical sections
    BOOL result = WaitOnAddress(
        (volatile void*)uaddr,
        &expected,
        sizeof(uint32_t),
        timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms
    );
    return result ? 0 : -1;
}

static inline long futex_wake(volatile uint32_t *uaddr, int num_wake) {
    if (num_wake == 1) {
        WakeByAddressSingle((void*)uaddr);
    } else {
        WakeByAddressAll((void*)uaddr);
    }
    return 0;
}

#else
// Linux futex
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

static inline long futex_wait_timeout(volatile uint32_t *uaddr, uint32_t expected, int timeout_ms) {
    if (timeout_ms < 0) {
        return syscall(SYS_futex, uaddr, FUTEX_WAIT_PRIVATE, expected, NULL);
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec  += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    return syscall(SYS_futex, uaddr, FUTEX_WAIT_PRIVATE, expected, &ts);
}

static inline long futex_wake(volatile uint32_t *uaddr, int num_wake) {
    return syscall(SYS_futex, uaddr, FUTEX_WAKE_PRIVATE, num_wake);
}

#endif

#endif // FUTEX_COMPAT_H
