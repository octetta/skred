#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>

// Timer control structure
typedef struct {
    void (*work)(void *);
    pthread_t timer_thread;    // Thread A
    pthread_t worker_thread;   // Thread B
    
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
    // Timing control
    volatile int64_t interval_ns;  // Interval in nanoseconds
    volatile int shutdown;
    
    // Precise timing - no drift compensation, just rock solid intervals
    struct timespec start_time;
    volatile uint64_t tick_count;
    
    // Statistics
    volatile int64_t max_jitter_ns;
    volatile int64_t total_jitter_ns;
} timer_control_t;

// Global timer control
static timer_control_t g_timer;

// Get current time as timespec
static inline void get_current_time(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

// Add nanoseconds to timespec
static inline void timespec_add_ns(struct timespec *ts, int64_t ns) {
    ts->tv_nsec += ns;
    while (ts->tv_nsec >= 1000000000L) {
        ts->tv_nsec -= 1000000000L;
        ts->tv_sec++;
    }
}

// Calculate difference between two timespecs in nanoseconds
static inline int64_t timespec_diff_ns(const struct timespec *later, const struct timespec *earlier) {
    return ((int64_t)(later->tv_sec - earlier->tv_sec)) * 1000000000L + 
           (later->tv_nsec - earlier->tv_nsec);
}

// Copy timespec
static inline void timespec_copy(struct timespec *dst, const struct timespec *src) {
    dst->tv_sec = src->tv_sec;
    dst->tv_nsec = src->tv_nsec;
}

// Thread B - Worker thread that performs actions
void* worker_thread(void* arg) {
    timer_control_t *tc = (timer_control_t*)arg;
    uint64_t local_tick = 0;
    
    printf("Worker thread ready\n");
    
    while (!tc->shutdown) {
        pthread_mutex_lock(&tc->mutex);
        
        // Wait for signal from timer thread
        while (local_tick >= tc->tick_count && !tc->shutdown) {
            pthread_cond_wait(&tc->cond, &tc->mutex);
        }
        
        if (tc->shutdown) {
            pthread_mutex_unlock(&tc->mutex);
            break;
        }
        
        local_tick = tc->tick_count;
        pthread_mutex_unlock(&tc->mutex);
        
        // YOUR SEQUENCER STEP GOES HERE
        //printf("♪ Sequencer step %lu (%.3fs)\n", local_tick, local_tick * tc->interval_ns / 1000000000.0);
        if (tc->work) tc->work(NULL);
        
        // Keep this section as fast as possible - no I/O, no heavy processing
    }
    
    printf("Worker thread stopped\n");
    return NULL;
}

// Thread A - Rock solid timer thread
void* timer_thread(void* arg) {
    timer_control_t *tc = (timer_control_t*)arg;
    struct timespec target_time, actual_time;
    int64_t jitter_ns;
    uint64_t tick = 0;
    
    printf("Timer thread started with %.3fms intervals\n", tc->interval_ns / 1000000.0);
    
    // Set the starting point
    timespec_copy(&target_time, &tc->start_time);
    
    while (!tc->shutdown) {
        tick++;
        
        // Calculate the exact time for this tick (no accumulated drift)
        timespec_copy(&target_time, &tc->start_time);
        timespec_add_ns(&target_time, tick * tc->interval_ns);
        
        // Sleep until the exact target time
        int result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &target_time, NULL);
        
        if (tc->shutdown) break;
        
        if (result != 0 && result != EINTR) {
            printf("clock_nanosleep failed: %s\n", strerror(result));
            continue;
        }
        
        // Measure actual timing precision
        get_current_time(&actual_time);
        jitter_ns = timespec_diff_ns(&actual_time, &target_time);
        if (jitter_ns < 0) jitter_ns = -jitter_ns; // Absolute jitter
        
        // Update statistics
        if (jitter_ns > tc->max_jitter_ns) {
            tc->max_jitter_ns = jitter_ns;
        }
        tc->total_jitter_ns += jitter_ns;
        
        // Signal worker thread - do this AFTER timing measurements
        pthread_mutex_lock(&tc->mutex);
        tc->tick_count = tick;
        pthread_cond_signal(&tc->cond);
        pthread_mutex_unlock(&tc->mutex);
        
        // Handle interval changes by resetting the base time
        // This prevents timing jumps when changing intervals
        static int64_t last_interval = 0;
        if (tc->interval_ns != last_interval && last_interval != 0) {
            get_current_time(&tc->start_time);
            tick = 0;
            last_interval = tc->interval_ns;
            printf("Interval changed to %.3fms - timing reset\n", tc->interval_ns / 1000000.0);
        } else if (last_interval == 0) {
            last_interval = tc->interval_ns;
        }
    }
    
    printf("Timer thread stopped\n");
    return NULL;
}

// Initialize the timer system
int timer_init(int64_t interval_ns, void (*work)(void *)) {
    if (interval_ns < 1000000LL || interval_ns > 31536000000000000LL) {
        fprintf(stderr, "Invalid interval: must be between 1ms and 1 year\n");
        return -1;
    }
    
    memset(&g_timer, 0, sizeof(g_timer));
    
    g_timer.work = work;
    g_timer.interval_ns = interval_ns;
    g_timer.shutdown = 0;
    g_timer.tick_count = 0;
    g_timer.max_jitter_ns = 0;
    g_timer.total_jitter_ns = 0;
    
    if (pthread_mutex_init(&g_timer.mutex, NULL) != 0) {
        perror("mutex init failed");
        return -1;
    }
    
    if (pthread_cond_init(&g_timer.cond, NULL) != 0) {
        perror("condition variable init failed");
        pthread_mutex_destroy(&g_timer.mutex);
        return -1;
    }
    
    // Set the absolute start time
    get_current_time(&g_timer.start_time);
    
    return 0;
}

// Start the timer threads
int timer_start(void) {
    // Create worker thread
    if (pthread_create(&g_timer.worker_thread, NULL, worker_thread, &g_timer) != 0) {
        perror("Failed to create worker thread");
        return -1;
    }
    
    // Try to get better scheduling for timer thread
    pthread_attr_t attr;
    struct sched_param param;
    int use_better_sched = 0;
    
    if (pthread_attr_init(&attr) == 0) {
        // Try SCHED_RR (round-robin) which doesn't need root on some systems
        if (pthread_attr_setschedpolicy(&attr, SCHED_RR) == 0) {
            param.sched_priority = 1; // Minimal elevation
            if (pthread_attr_setschedparam(&attr, &param) == 0) {
                pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
                use_better_sched = 1;
            }
        } else {
          puts("_attr_setschedpolicy not 0");
        }
    } else {
      puts("_attr_init not 0");
    }
    
    // Create timer thread
    if (pthread_create(&g_timer.timer_thread, use_better_sched ? &attr : NULL, 
                       timer_thread, &g_timer) != 0) {
        if (use_better_sched) {
            pthread_attr_destroy(&attr);
            // Fall back to default scheduling
            if (pthread_create(&g_timer.timer_thread, NULL, timer_thread, &g_timer) != 0) {
                perror("Failed to create timer thread");
                g_timer.shutdown = 1;
                pthread_cond_signal(&g_timer.cond);
                pthread_join(g_timer.worker_thread, NULL);
                return -1;
            }
        } else {
            perror("Failed to create timer thread");
            g_timer.shutdown = 1;
            pthread_cond_signal(&g_timer.cond);
            pthread_join(g_timer.worker_thread, NULL);
            return -1;
        }
    }
    
    if (use_better_sched) {
        pthread_attr_destroy(&attr);
        printf("Timer using improved scheduling\n");
    } else {
        printf("Timer using default scheduling\n");
    }
    
    return 0;
}

// Change the timer interval dynamically
void timer_set_interval(int64_t interval_ns) {
    if (interval_ns >= 1000000LL && interval_ns <= 31536000000000000LL) {
        g_timer.interval_ns = interval_ns;
    }
}

// Get timing statistics
void timer_get_stats(uint64_t *tick_count, int64_t *max_jitter_ns, int64_t *avg_jitter_ns) {
    if (tick_count) *tick_count = g_timer.tick_count;
    if (max_jitter_ns) *max_jitter_ns = g_timer.max_jitter_ns;
    if (avg_jitter_ns) {
        *avg_jitter_ns = g_timer.tick_count > 0 ? 
            g_timer.total_jitter_ns / g_timer.tick_count : 0;
    }
}

// Stop and cleanup the timer system
void timer_stop(void) {
    printf("\nStopping timer system...\n");
    
    g_timer.shutdown = 1;
    
    // Wake up worker thread
    pthread_mutex_lock(&g_timer.mutex);
    pthread_cond_signal(&g_timer.cond);
    pthread_mutex_unlock(&g_timer.mutex);
    
    // Wait for threads to finish
    pthread_join(g_timer.timer_thread, NULL);
    pthread_join(g_timer.worker_thread, NULL);
    
    // Cleanup
    pthread_mutex_destroy(&g_timer.mutex);
    pthread_cond_destroy(&g_timer.cond);
    
    printf("Timer system stopped\n");
}

#ifdef DEMO
void tick(void *v) {
  puts("TICK");
}
// Example usage and testing
int main(void) {
    uint64_t tick_count;
    int64_t max_jitter, avg_jitter;
    
    printf("Rock-Solid Sequencer Timer\n");
    printf("==========================\n");
    
    // Test with your 250ms sequencer timing
    if (timer_init(250000000LL, tick) != 0) { // 250ms
        fprintf(stderr, "Failed to initialize timer\n");
        return 1;
    }
    
    if (timer_start() != 0) {
        fprintf(stderr, "Failed to start timer\n");
        return 1;
    }
    
    printf("\nSequencer running... (Ctrl+C to stop or wait 10 seconds)\n");
    printf("Listen for consistent ♪ beats every 250ms\n\n");
    
    // Let it run for 10 seconds
    sleep(10);
    
    // Get final statistics
    timer_get_stats(&tick_count, &max_jitter, &avg_jitter);
    
    printf("\n" "=" "40\n");
    printf("SEQUENCER TIMING ANALYSIS\n");
    printf("Total steps: %lu (expected: 40)\n", tick_count);
    printf("Max jitter: %ld ns (%.3f ms)\n", max_jitter, max_jitter / 1000000.0);
    printf("Avg jitter: %ld ns (%.3f ms)\n", avg_jitter, avg_jitter / 1000000.0);
    
    if (avg_jitter < 100000) { // < 0.1ms
        printf("✓ EXCELLENT timing for sequencer use\n");
    } else if (avg_jitter < 1000000) { // < 1ms
        printf("✓ GOOD timing for sequencer use\n");
    } else {
        printf("⚠ Timing may need improvement for tight sequencing\n");
    }
    
    timer_stop();
    return 0;
}
#endif
