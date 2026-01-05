// futex-compat.h - Cross-platform futex-like implementation
#ifndef FUTEX_COMPAT_H
#define FUTEX_COMPAT_H

inline long futex_wait_timeout(volatile uint32_t *uaddr, uint32_t expected, int timeout_ms);
inline long futex_wake(volatile uint32_t *uaddr, int num_wake);

#endif // FUTEX_COMPAT_H
