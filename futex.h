#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>

// A safe wrapper for the futex system call
// Note: This only handles the common FUTEX_WAIT and FUTEX_WAKE operations.
static inline int futex_wait(volatile int *uaddr, int val) {
  return syscall(SYS_futex, uaddr, FUTEX_WAIT, val, NULL, NULL, 0);
}

static inline int futex_wake(volatile int *uaddr, int num) {
  return syscall(SYS_futex, uaddr, FUTEX_WAKE, num, NULL, NULL, 0);
}
