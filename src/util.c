#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <processthreadsapi.h>
#include <wchar.h>
#else
#include <pthread.h>
#endif

void util_set_thread_name(char *s) {
#ifdef _WIN32
  wchar_t name[64];
  swprintf(name, sizeof(name), L"%s", s);
  SetThreadDescription(GetCurrentThread(), name);
#else
#ifdef __APPLE__
  pthread_setname_np(s);
#else
  pthread_setname_np(pthread_self(), s);
#endif
#endif
}

#include <stdint.h>
#include <time.h>

int64_t ts_diff_ns(const struct timespec *a, const struct timespec *b) {
  return ((int64_t)b->tv_sec  - a->tv_sec)  * 1000000000LL +
    ((int64_t)b->tv_nsec - a->tv_nsec);
}

