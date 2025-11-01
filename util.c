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
  pthread_setname_np(pthread_self(), s);
#endif
}
