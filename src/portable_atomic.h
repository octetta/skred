#ifndef PORTABLE_ATOMIC_H
#define PORTABLE_ATOMIC_H

#include <stdint.h>

/* Portable atomic operations for macOS, Linux, and Windows */

#if defined(_WIN32) || defined(_WIN64)
    /* Windows */
    #include <windows.h>
    #include <intrin.h>
    
    typedef volatile LONG atomic_int_t;
    typedef volatile LONGLONG atomic_uint64_t;
    
    static inline int atomic_load_int(atomic_int_t *ptr) {
        return InterlockedOr((LONG*)ptr, 0);
    }
    
    static inline void atomic_store_int(atomic_int_t *ptr, int val) {
        InterlockedExchange((LONG*)ptr, val);
    }
    
    static inline int atomic_compare_exchange_int(atomic_int_t *ptr, int *expected, int desired) {
        LONG old = InterlockedCompareExchange((LONG*)ptr, desired, *expected);
        int success = (old == *expected);
        *expected = old;
        return success;
    }
    
    static inline int atomic_fetch_add_int(atomic_int_t *ptr, int val) {
        return InterlockedExchangeAdd((LONG*)ptr, val);
    }
    
    static inline uint64_t atomic_fetch_add_uint64(atomic_uint64_t *ptr, uint64_t val) {
        return (uint64_t)InterlockedExchangeAdd64((LONGLONG*)ptr, val);
    }
    
    static inline void atomic_store_uint64(atomic_uint64_t *ptr, uint64_t val) {
        InterlockedExchange64((LONGLONG*)ptr, val);
    }
    
    static inline uint64_t atomic_load_uint64(atomic_uint64_t *ptr) {
        return (uint64_t)InterlockedOr64((LONGLONG*)ptr, 0);
    }
    
#else
    /* GCC/Clang atomics (macOS, Linux) */
    typedef volatile int atomic_int_t;
    typedef volatile uint64_t atomic_uint64_t;
    
    static inline int atomic_load_int(atomic_int_t *ptr) {
        return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
    }
    
    static inline void atomic_store_int(atomic_int_t *ptr, int val) {
        __atomic_store_n(ptr, val, __ATOMIC_RELEASE);
    }
    
    static inline int atomic_compare_exchange_int(atomic_int_t *ptr, int *expected, int desired) {
        return __atomic_compare_exchange_n(ptr, expected, desired, 0, 
                                           __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    }
    
    static inline int atomic_fetch_add_int(atomic_int_t *ptr, int val) {
        return __atomic_fetch_add(ptr, val, __ATOMIC_ACQ_REL);
    }
    
    static inline uint64_t atomic_fetch_add_uint64(atomic_uint64_t *ptr, uint64_t val) {
        return __atomic_fetch_add(ptr, val, __ATOMIC_ACQ_REL);
    }
    
    static inline void atomic_store_uint64(atomic_uint64_t *ptr, uint64_t val) {
        __atomic_store_n(ptr, val, __ATOMIC_RELEASE);
    }
    
    static inline uint64_t atomic_load_uint64(atomic_uint64_t *ptr) {
        return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
    }
#endif

/* Memory barrier */
#if defined(_WIN32) || defined(_WIN64)
    #define MEMORY_BARRIER() MemoryBarrier()
#else
    #define MEMORY_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif

/* Simple mutex for non-critical operations */
#if defined(_WIN32) || defined(_WIN64)
    typedef CRITICAL_SECTION simple_mutex_t;
    
    static inline void simple_mutex_init(simple_mutex_t *m) {
        InitializeCriticalSection(m);
    }
    
    static inline void simple_mutex_lock(simple_mutex_t *m) {
        EnterCriticalSection(m);
    }
    
    static inline void simple_mutex_unlock(simple_mutex_t *m) {
        LeaveCriticalSection(m);
    }
    
    static inline void simple_mutex_destroy(simple_mutex_t *m) {
        DeleteCriticalSection(m);
    }
#else
    #include <pthread.h>
    typedef pthread_mutex_t simple_mutex_t;
    
    static inline void simple_mutex_init(simple_mutex_t *m) {
        pthread_mutex_init(m, NULL);
    }
    
    static inline void simple_mutex_lock(simple_mutex_t *m) {
        pthread_mutex_lock(m);
    }
    
    static inline void simple_mutex_unlock(simple_mutex_t *m) {
        pthread_mutex_unlock(m);
    }
    
    static inline void simple_mutex_destroy(simple_mutex_t *m) {
        pthread_mutex_destroy(m);
    }
#endif

#endif /* PORTABLE_ATOMIC_H */
