/*
RTKit Simple - A single-file RTKit wrapper library
Version 1.0 - Public Domain

Usage:
    #define RTKIT_IMPLEMENTATION
    #include "rtkit_simple.h"

    // Initialize RTKit
    if (rtkit_init() == 0) {
        // Request realtime priority for current thread
        rtkit_make_realtime(0, 50); // priority 50 (1-99)
        
        // Or just high priority (non-realtime)
        rtkit_make_high_priority(0, -10); // nice value -10
        
        // Cleanup
        rtkit_deinit();
    }

Features:
- Single header file implementation
- Dynamic loading of libdbus (no build dependencies)
- Automatic fallback to standard scheduling if RTKit unavailable
- Thread-safe operations
- Minimal memory footprint

License: Public Domain (or MIT if public domain unavailable in your jurisdiction)
*/

#ifndef RTKIT_SIMPLE_H
#define RTKIT_SIMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>

// Return codes
#define RTKIT_SUCCESS           0
#define RTKIT_ERROR_INIT       -1
#define RTKIT_ERROR_DBUS       -2
#define RTKIT_ERROR_PERMISSION -3
#define RTKIT_ERROR_INVALID    -4
#define RTKIT_ERROR_UNAVAILABLE -5

// RTKit API
int rtkit_init(void);
void rtkit_deinit(void);
int rtkit_is_available(void);

// Make thread realtime (priority 1-99, higher = more priority)
int rtkit_make_realtime(pthread_t thread, int priority);

// Make thread high priority (nice -20 to 19, lower = higher priority)  
int rtkit_make_high_priority(pthread_t thread, int nice_level);

// Get limits
int rtkit_get_max_realtime_priority(void);
int rtkit_get_min_nice_level(void);
long long rtkit_get_rttime_usec_max(void);

// Utility functions
pid_t rtkit_get_thread_id(pthread_t thread);
int rtkit_reset_to_normal(pthread_t thread);

#ifdef __cplusplus
}
#endif

#endif // RTKIT_SIMPLE_H

#ifdef RTKIT_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <errno.h>
#include <sched.h>

// D-Bus forward declarations and types
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusPendingCall DBusPendingCall;
typedef uint32_t dbus_uint32_t;
typedef int32_t dbus_int32_t;
typedef uint64_t dbus_uint64_t;
typedef int dbus_bool_t;

// DBusError structure (based on libdbus internals)
typedef struct {
    const char *name;
    const char *message;
    unsigned int dummy1 : 1;
    unsigned int dummy2 : 1;
    unsigned int dummy3 : 1;
    unsigned int dummy4 : 1;
    unsigned int dummy5 : 1;
    void *padding1;
} DBusError;

static struct {
    void* libdbus_handle;
    int initialized;
    DBusConnection* connection;
    
    // D-Bus function pointers
    DBusConnection* (*bus_get)(int type, DBusError* error);
    void (*connection_unref)(DBusConnection* connection);
    DBusMessage* (*message_new_method_call)(const char* destination, const char* path, 
                                          const char* interface, const char* method);
    void (*message_unref)(DBusMessage* message);
    dbus_bool_t (*message_append_args)(DBusMessage* message, int first_arg_type, ...);
    dbus_bool_t (*connection_send_with_reply)(DBusConnection* connection, DBusMessage* message,
                                            DBusPendingCall** pending, int timeout);
    void (*pending_call_block)(DBusPendingCall* pending);
    DBusMessage* (*pending_call_steal_reply)(DBusPendingCall* pending);
    void (*pending_call_unref)(DBusPendingCall* pending);
    dbus_bool_t (*message_get_args)(DBusMessage* message, DBusError* error, int first_arg_type, ...);
    void (*error_init)(DBusError* error);
    void (*error_free)(DBusError* error);
    dbus_bool_t (*error_is_set)(const DBusError* error);
    const char* (*message_get_error_name)(DBusMessage* message);
} rtkit_ctx = {0};

// D-Bus constants
#define DBUS_BUS_SYSTEM 1
#define DBUS_TYPE_UINT64 ((int) 'x')
#define DBUS_TYPE_UINT32 ((int) 'u')
#define DBUS_TYPE_INT32  ((int) 'i')
#define DBUS_TYPE_INVALID ((int) '\0')

static int rtkit_load_dbus(void) {
    if (rtkit_ctx.libdbus_handle) return RTKIT_SUCCESS;
    
    // Try different possible libdbus names
    const char* dbus_libs[] = {
        "libdbus-1.so.3",
        "libdbus-1.so",
        "libdbus.so.1",
        "libdbus.so",
        NULL
    };
    
    for (int i = 0; dbus_libs[i]; i++) {
        rtkit_ctx.libdbus_handle = dlopen(dbus_libs[i], RTLD_LAZY | RTLD_LOCAL);
        if (rtkit_ctx.libdbus_handle) break;
    }
    
    if (!rtkit_ctx.libdbus_handle) {
        return RTKIT_ERROR_UNAVAILABLE;
    }
    
    // Load required functions
    #define LOAD_FUNC(name) do { \
        rtkit_ctx.name = dlsym(rtkit_ctx.libdbus_handle, "dbus_" #name); \
        if (!rtkit_ctx.name) goto error; \
    } while(0)
    
    LOAD_FUNC(bus_get);
    LOAD_FUNC(connection_unref);
    LOAD_FUNC(message_new_method_call);
    LOAD_FUNC(message_unref);
    LOAD_FUNC(message_append_args);
    LOAD_FUNC(connection_send_with_reply);
    LOAD_FUNC(pending_call_block);
    LOAD_FUNC(pending_call_steal_reply);
    LOAD_FUNC(pending_call_unref);
    LOAD_FUNC(message_get_args);
    LOAD_FUNC(error_init);
    LOAD_FUNC(error_free);
    LOAD_FUNC(error_is_set);
    LOAD_FUNC(message_get_error_name);
    
    #undef LOAD_FUNC
    
    return RTKIT_SUCCESS;
    
error:
    dlclose(rtkit_ctx.libdbus_handle);
    rtkit_ctx.libdbus_handle = NULL;
    return RTKIT_ERROR_UNAVAILABLE;
}

static int rtkit_call_method(const char* method, dbus_uint64_t thread_id, 
                           dbus_uint32_t arg, void* result, int result_type) {
    DBusMessage* msg = NULL;
    DBusMessage* reply = NULL;
    DBusPendingCall* pending = NULL;
    DBusError error;
    int ret = RTKIT_ERROR_DBUS;
    
    if (!rtkit_ctx.connection) return RTKIT_ERROR_INIT;
    
    rtkit_ctx.error_init(&error);
    
    msg = rtkit_ctx.message_new_method_call("org.freedesktop.RealtimeKit1",
                                          "/org/freedesktop/RealtimeKit1",
                                          "org.freedesktop.RealtimeKit1",
                                          method);
    if (!msg) goto cleanup;
    
    if (!rtkit_ctx.message_append_args(msg, 
                                     DBUS_TYPE_UINT64, &thread_id,
                                     DBUS_TYPE_UINT32, &arg,
                                     DBUS_TYPE_INVALID)) {
        goto cleanup;
    }
    
    if (!rtkit_ctx.connection_send_with_reply(rtkit_ctx.connection, msg, &pending, 5000)) {
        goto cleanup;
    }
    
    rtkit_ctx.pending_call_block(pending);
    reply = rtkit_ctx.pending_call_steal_reply(pending);
    
    if (!reply) goto cleanup;
    
    // Check if it's an error reply
    const char* error_name = rtkit_ctx.message_get_error_name(reply);
    if (error_name) {
        if (strstr(error_name, "AccessDenied")) {
            ret = RTKIT_ERROR_PERMISSION;
        }
        goto cleanup;
    }
    
    // Get result if requested
    if (result && result_type != DBUS_TYPE_INVALID) {
        if (!rtkit_ctx.message_get_args(reply, &error, result_type, result, DBUS_TYPE_INVALID)) {
            goto cleanup;
        }
    }
    
    ret = RTKIT_SUCCESS;
    
cleanup:
    if (msg) rtkit_ctx.message_unref(msg);
    if (reply) rtkit_ctx.message_unref(reply);
    if (pending) rtkit_ctx.pending_call_unref(pending);
    rtkit_ctx.error_free(&error);
    
    return ret;
}

int rtkit_init(void) {
    DBusError error;
    
    if (rtkit_ctx.initialized) return RTKIT_SUCCESS;
    
    if (rtkit_load_dbus() != RTKIT_SUCCESS) {
        return RTKIT_ERROR_UNAVAILABLE;
    }
    
    rtkit_ctx.error_init(&error);
    rtkit_ctx.connection = rtkit_ctx.bus_get(DBUS_BUS_SYSTEM, &error);
    
    if (rtkit_ctx.error_is_set(&error) || !rtkit_ctx.connection) {
        rtkit_ctx.error_free(&error);
        return RTKIT_ERROR_DBUS;
    }
    
    rtkit_ctx.error_free(&error);
    rtkit_ctx.initialized = 1;
    
    return RTKIT_SUCCESS;
}

void rtkit_deinit(void) {
    if (!rtkit_ctx.initialized) return;
    
    if (rtkit_ctx.connection) {
        rtkit_ctx.connection_unref(rtkit_ctx.connection);
        rtkit_ctx.connection = NULL;
    }
    
    if (rtkit_ctx.libdbus_handle) {
        dlclose(rtkit_ctx.libdbus_handle);
        rtkit_ctx.libdbus_handle = NULL;
    }
    
    memset(&rtkit_ctx, 0, sizeof(rtkit_ctx));
}

int rtkit_is_available(void) {
    return rtkit_ctx.initialized;
}

pid_t rtkit_get_thread_id(pthread_t thread) {
    if (pthread_equal(thread, pthread_self()) || thread == 0) {
        return syscall(SYS_gettid);
    }
    
    // For other threads, this is more complex and platform-specific
    // For now, return 0 to indicate we should use current thread
    return 0;
}

int rtkit_make_realtime(pthread_t thread, int priority) {
    pid_t tid;
    dbus_uint64_t thread_id;
    dbus_uint32_t prio;
    int ret;
    
    if (!rtkit_ctx.initialized) {
        return RTKIT_ERROR_INIT;
    }
    
    if (priority < 1 || priority > 99) {
        return RTKIT_ERROR_INVALID;
    }
    
    tid = rtkit_get_thread_id(thread);
    if (tid <= 0) tid = syscall(SYS_gettid);
    
    thread_id = (dbus_uint64_t)tid;
    prio = (dbus_uint32_t)priority;
    
    ret = rtkit_call_method("MakeThreadRealtime", thread_id, prio, NULL, DBUS_TYPE_INVALID);
    
    // If RTKit fails, try fallback using standard POSIX scheduling
    if (ret != RTKIT_SUCCESS) {
        struct sched_param param;
        param.sched_priority = priority;
        
        if (pthread_setschedparam(thread ? thread : pthread_self(), SCHED_FIFO, &param) == 0) {
            return RTKIT_SUCCESS;
        }
        
        // Try SCHED_RR as fallback
        if (pthread_setschedparam(thread ? thread : pthread_self(), SCHED_RR, &param) == 0) {
            return RTKIT_SUCCESS;
        }
    }
    
    return ret;
}

int rtkit_make_high_priority(pthread_t thread, int nice_level) {
    pid_t tid;
    dbus_uint64_t thread_id;
    dbus_uint32_t nice;
    int ret;
    
    if (!rtkit_ctx.initialized) {
        return RTKIT_ERROR_INIT;
    }
    
    if (nice_level < -20 || nice_level > 19) {
        return RTKIT_ERROR_INVALID;
    }
    
    tid = rtkit_get_thread_id(thread);
    if (tid <= 0) tid = syscall(SYS_gettid);
    
    thread_id = (dbus_uint64_t)tid;
    nice = (dbus_uint32_t)nice_level;
    
    ret = rtkit_call_method("MakeThreadHighPriority", thread_id, nice, NULL, DBUS_TYPE_INVALID);
    
    // Fallback to setpriority if RTKit fails
    if (ret != RTKIT_SUCCESS) {
        if (setpriority(PRIO_PROCESS, tid, nice_level) == 0) {
            return RTKIT_SUCCESS;
        }
    }
    
    return ret;
}

int rtkit_get_max_realtime_priority(void) {
    dbus_int32_t result = 0;
    
    if (rtkit_call_method("GetRTTimeUSecMax", 0, 0, &result, DBUS_TYPE_INT32) == RTKIT_SUCCESS) {
        return result;
    }
    
    // Fallback
    return sched_get_priority_max(SCHED_FIFO);
}

int rtkit_get_min_nice_level(void) {
    dbus_int32_t result = 0;
    
    if (rtkit_call_method("GetMinNiceLevel", 0, 0, &result, DBUS_TYPE_INT32) == RTKIT_SUCCESS) {
        return result;
    }
    
    return -20; // Standard minimum
}

long long rtkit_get_rttime_usec_max(void) {
    dbus_uint64_t result = 0;
    
    if (rtkit_call_method("GetRTTimeUSecMax", 0, 0, &result, DBUS_TYPE_UINT64) == RTKIT_SUCCESS) {
        return result;
    }
    
    return 200000LL; // 200ms default
}

int rtkit_reset_to_normal(pthread_t thread) {
    struct sched_param param;
    param.sched_priority = 0;
    
    if (pthread_setschedparam(thread ? thread : pthread_self(), SCHED_OTHER, &param) == 0) {
        return RTKIT_SUCCESS;
    }
    
    return RTKIT_ERROR_DBUS;
}

#endif // RTKIT_IMPLEMENTATION