/* mpsc_queue.h - v1.0 - Public Domain MPSC Queue for Real-Time Audio
 *
 * A lock-free multi-producer single-consumer queue for sending short strings
 * from real-time audio callbacks to a consumer thread.
 *
 * USAGE:
 *   #define MPSC_QUEUE_IMPLEMENTATION
 *   #include "mpsc_queue.h"
 *
 * FEATURES:
 *   - Lock-free for producers (safe for audio callbacks)
 *   - Blocking consumer (efficient, no CPU waste)
 *   - Cross-platform (Windows, Linux, macOS)
 *   - No dynamic allocation
 *   - Public domain
 *
 * EXAMPLE:
 *   mpsc_queue queue;
 *   mpsc_queue_init(&queue);
 *   
 *   // In audio callback (lock-free):
 *   mpsc_queue_send(&queue, "audio_event");
 *   
 *   // In consumer thread (blocks efficiently):
 *   char msg[MPSC_MAX_STRING_LEN];
 *   while (mpsc_queue_receive(&queue, msg, sizeof(msg))) {
 *       handle_message(msg);
 *   }
 *   
 *   mpsc_queue_destroy(&queue);
 *
 * CONFIGURATION:
 *   #define MPSC_MAX_STRING_LEN 256  // Max string length (before including)
 *   #define MPSC_RING_SIZE 64        // Queue capacity, must be power of 2
 *
 * LICENSE:
 *   This software is dual-licensed to the public domain and under the following
 *   license: you are granted a perpetual, irrevocable license to copy, modify,
 *   publish, and distribute this file as you see fit.
 */

#ifndef MPSC_QUEUE_H
#define MPSC_QUEUE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#ifndef MPSC_MAX_STRING_LEN
#define MPSC_MAX_STRING_LEN 256
#endif

#ifndef MPSC_RING_SIZE
#define MPSC_RING_SIZE 64
#endif

/* Platform-specific semaphore */
#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE mpsc_sem_t;
#else
    #include <semaphore.h>
    typedef sem_t mpsc_sem_t;
#endif

/* Queue slot */
typedef struct {
    char data[MPSC_MAX_STRING_LEN];
    atomic_bool ready;
} mpsc_slot;

/* Queue structure */
typedef struct {
    mpsc_slot slots[MPSC_RING_SIZE];
    atomic_size_t write_pos;
    atomic_size_t read_pos;
    mpsc_sem_t sem;
} mpsc_queue;

/* Initialize queue - call once at startup */
bool mpsc_queue_init(mpsc_queue *q);

/* Destroy queue - call at shutdown */
void mpsc_queue_destroy(mpsc_queue *q);

/* Send message (lock-free, non-blocking) - safe for audio callbacks
 * Returns true on success, false if queue is full */
bool mpsc_queue_send(mpsc_queue *q, const char *str);

/* Receive message (blocking) - consumer waits efficiently without CPU usage
 * Returns true on success, false on error */
bool mpsc_queue_receive(mpsc_queue *q, char *out, size_t out_size);

/* Try to receive without blocking
 * Returns true if message received, false if queue is empty */
bool mpsc_queue_try_receive(mpsc_queue *q, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* MPSC_QUEUE_H */

/* ============================================================================
 *                            IMPLEMENTATION
 * ============================================================================ */

#ifdef MPSC_QUEUE_IMPLEMENTATION

#include <string.h>

bool mpsc_queue_init(mpsc_queue *q) {
    atomic_init(&q->write_pos, 0);
    atomic_init(&q->read_pos, 0);
    
    for (int i = 0; i < MPSC_RING_SIZE; i++) {
        atomic_init(&q->slots[i].ready, false);
    }
    
#ifdef _WIN32
    q->sem = CreateSemaphore(NULL, 0, MPSC_RING_SIZE, NULL);
    return q->sem != NULL;
#else
    return sem_init(&q->sem, 0, 0) == 0;
#endif
}

void mpsc_queue_destroy(mpsc_queue *q) {
#ifdef _WIN32
    CloseHandle(q->sem);
#else
    sem_destroy(&q->sem);
#endif
}

bool mpsc_queue_send(mpsc_queue *q, const char *str) {
    /* Atomically claim a slot */
    size_t write = atomic_fetch_add_explicit(&q->write_pos, 1, 
                                             memory_order_relaxed);
    size_t index = write & (MPSC_RING_SIZE - 1);
    
    /* Check if queue is full */
    size_t read = atomic_load_explicit(&q->read_pos, memory_order_acquire);
    size_t distance = write - read;
    if (distance >= MPSC_RING_SIZE) {
        return false;
    }
    
    /* Wait for slot to be consumed (brief spin with timeout) */
    bool expected = false;
    int spin_count = 0;
    while (!atomic_compare_exchange_weak_explicit(
        &q->slots[index].ready, &expected, false,
        memory_order_acquire, memory_order_relaxed)) {
        expected = false;
        if (++spin_count > 100) {
            return false;  /* Timeout for RT safety */
        }
    }
    
    /* Copy string into slot */
    strncpy(q->slots[index].data, str, MPSC_MAX_STRING_LEN - 1);
    q->slots[index].data[MPSC_MAX_STRING_LEN - 1] = '\0';
    
    /* Mark slot as ready */
    atomic_store_explicit(&q->slots[index].ready, true, memory_order_release);
    
    /* Signal consumer (non-blocking) */
#ifdef _WIN32
    ReleaseSemaphore(q->sem, 1, NULL);
#else
    sem_post(&q->sem);
#endif
    
    return true;
}

bool mpsc_queue_receive(mpsc_queue *q, char *out, size_t out_size) {
    /* Wait for data (blocks without CPU usage) */
#ifdef _WIN32
    if (WaitForSingleObject(q->sem, INFINITE) != WAIT_OBJECT_0) {
        return false;
    }
#else
    if (sem_wait(&q->sem) != 0) {
        return false;
    }
#endif
    
    /* Get read position */
    size_t read = atomic_load_explicit(&q->read_pos, memory_order_relaxed);
    size_t index = read & (MPSC_RING_SIZE - 1);
    
    /* Wait briefly if producer hasn't finished writing (rare) */
    int spin = 0;
    while (!atomic_load_explicit(&q->slots[index].ready, memory_order_acquire)) {
        if (++spin > 1000) {
            return false;  /* Safety timeout */
        }
    }
    
    /* Copy string out */
    strncpy(out, q->slots[index].data, out_size - 1);
    out[out_size - 1] = '\0';
    
    /* Mark slot as consumed and advance */
    atomic_store_explicit(&q->slots[index].ready, false, memory_order_release);
    atomic_store_explicit(&q->read_pos, read + 1, memory_order_release);
    
    return true;
}

bool mpsc_queue_try_receive(mpsc_queue *q, char *out, size_t out_size) {
    /* Try to decrement semaphore without blocking */
#ifdef _WIN32
    if (WaitForSingleObject(q->sem, 0) != WAIT_OBJECT_0) {
        return false;
    }
#else
    if (sem_trywait(&q->sem) != 0) {
        return false;
    }
#endif
    
    size_t read = atomic_load_explicit(&q->read_pos, memory_order_relaxed);
    size_t index = read & (MPSC_RING_SIZE - 1);
    
    if (!atomic_load_explicit(&q->slots[index].ready, memory_order_acquire)) {
        return false;
    }
    
    strncpy(out, q->slots[index].data, out_size - 1);
    out[out_size - 1] = '\0';
    
    atomic_store_explicit(&q->slots[index].ready, false, memory_order_release);
    atomic_store_explicit(&q->read_pos, read + 1, memory_order_release);
    
    return true;
}

#endif /* MPSC_QUEUE_IMPLEMENTATION */

/*
------------------------------------------------------------------------------
USAGE EXAMPLE:
------------------------------------------------------------------------------

// In ONE .c file:
#define MPSC_QUEUE_IMPLEMENTATION
#include "mpsc_queue.h"

// In your code:
mpsc_queue g_queue;

void setup() {
    mpsc_queue_init(&g_queue);
}

// Multiple audio callbacks (lock-free):
void audio_callback_1(float *buffer, int frames) {
    if (detected_event) {
        mpsc_queue_send(&g_queue, "event_from_callback1");
    }
}

void audio_callback_2(float *buffer, int frames) {
    if (other_event) {
        mpsc_queue_send(&g_queue, "event_from_callback2");
    }
}

// Consumer thread (blocks efficiently):
void* consumer_thread(void* arg) {
    char msg[MPSC_MAX_STRING_LEN];
    
    while (running) {
        // Blocks here with zero CPU until data arrives
        if (mpsc_queue_receive(&g_queue, msg, sizeof(msg))) {
            printf("Received: %s\n", msg);
            handle_message(msg);
        }
    }
    return NULL;
}

void cleanup() {
    mpsc_queue_destroy(&g_queue);
}

------------------------------------------------------------------------------
COMPILE:
    Linux/macOS:  gcc -std=c11 -pthread yourfile.c
    Windows:      cl /std:c11 yourfile.c
------------------------------------------------------------------------------
*/
