#include <time.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

#include "seq.h"

// Configuration
#define SAMPLE_RATE 44100
#define SAMPLES_PER_CALLBACK 2048
#define CALLBACK_INTERVAL_NS (1000000000LL * SAMPLES_PER_CALLBACK / SAMPLE_RATE)
#define MAX_EVENTS 1024
#define MIN_BPM 1.0
#define MAX_BPM 600.0

// Timing state (from previous algorithm)
typedef struct {
    atomic_uint_fast64_t callback_count;
    atomic_uint_fast64_t last_callback_time_ns;
} timing_state_t;

static timing_state_t g_timing_state = {0};

// Sequencer state
typedef struct {
    atomic_uint_fast64_t start_time_ns;     // When sequencer started
    atomic_uint_fast64_t current_beat_ns;   // Current beat position in nanoseconds
    atomic_uint_fast32_t bpm_fixed;         // BPM * 1000 for sub-BPM precision
    atomic_bool running;
    atomic_uint_fast32_t beat_count;        // Current beat number (for display/sync)
} sequencer_state_t;

// Event structure
typedef struct {
    uint64_t beat_time_ns;    // When to trigger (in beat time)
    int event_id;             // Event identifier
    bool active;              // Whether this event slot is in use
} sequencer_event_t;

// Global sequencer state
static sequencer_state_t g_sequencer = {0};
static sequencer_event_t g_events[MAX_EVENTS] = {0};

// Audio callback (unchanged from previous)
void inside_audio_callback(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_ns = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    
    atomic_store(&g_timing_state.last_callback_time_ns, now_ns);
    atomic_fetch_add(&g_timing_state.callback_count, 1);
}

// Get accurate current time (unchanged from previous)
uint64_t get_accurate_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t system_time_ns = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    
    uint64_t callback_count = atomic_load(&g_timing_state.callback_count);
    uint64_t last_callback_time_ns = atomic_load(&g_timing_state.last_callback_time_ns);
    
    if (callback_count == 0 || last_callback_time_ns == 0) {
        return system_time_ns;
    }
    
    uint64_t time_since_callback = system_time_ns - last_callback_time_ns;
    
    if (time_since_callback > CALLBACK_INTERVAL_NS * 2) {
        return system_time_ns;
    }
    
    uint64_t audio_reference_time = callback_count * CALLBACK_INTERVAL_NS;
    return audio_reference_time + time_since_callback;
}

// Convert BPM to nanoseconds per beat
uint64_t bpm_to_beat_interval_ns(double bpm) {
    if (bpm < MIN_BPM) bpm = MIN_BPM;
    if (bpm > MAX_BPM) bpm = MAX_BPM;
    
    // 60 seconds per minute = 60,000,000,000 ns per minute
    return (uint64_t)(60000000000.0 / bpm);
}

// Start the sequencer
void sequencer_start(double bpm) {
    uint64_t now = get_accurate_time_ns();
    uint32_t bpm_fixed = (uint32_t)(bpm * 1000); // Store BPM * 1000 for precision
    
    atomic_store(&g_sequencer.start_time_ns, now);
    atomic_store(&g_sequencer.current_beat_ns, 0);
    atomic_store(&g_sequencer.bpm_fixed, bpm_fixed);
    atomic_store(&g_sequencer.beat_count, 0);
    atomic_store(&g_sequencer.running, true);
}

// Stop the sequencer
void sequencer_stop(void) {
    atomic_store(&g_sequencer.running, false);
}

// Change BPM during playback
void sequencer_set_bpm(double bpm) {
    uint32_t bpm_fixed = (uint32_t)(bpm * 1000);
    atomic_store(&g_sequencer.bpm_fixed, bpm_fixed);
}

// Get current beat position (fractional beats from start)
double sequencer_get_beat_position(void) {
    if (!atomic_load(&g_sequencer.running)) {
        return 0.0;
    }
    
    uint64_t now = get_accurate_time_ns();
    uint64_t start_time = atomic_load(&g_sequencer.start_time_ns);
    uint32_t bpm_fixed = atomic_load(&g_sequencer.bpm_fixed);
    
    double bpm = bpm_fixed / 1000.0;
    uint64_t elapsed_ns = now - start_time;
    
    // Convert elapsed time to beats
    double beats = (double)elapsed_ns * bpm / 60000000000.0;
    return beats;
}

// Schedule an event at a specific beat
int sequencer_schedule_event(double beat_time, int event_id) {
    // Find empty slot
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (!g_events[i].active) {
            uint32_t bpm_fixed = atomic_load(&g_sequencer.bpm_fixed);
            double bpm = bpm_fixed / 1000.0;
            
            // Convert beat time to nanoseconds from sequencer start
            g_events[i].beat_time_ns = (uint64_t)(beat_time * 60000000000.0 / bpm);
            g_events[i].event_id = event_id;
            g_events[i].active = true;
            return i; // Return slot index
        }
    }
    return -1; // No free slots
}

// Remove a scheduled event
void sequencer_remove_event(int slot_index) {
    if (slot_index >= 0 && slot_index < MAX_EVENTS) {
        g_events[slot_index].active = false;
    }
}

// Main sequencer update - call this frequently from your timing thread
typedef struct {
    int event_id;
    double beat_time;
    uint64_t actual_time_ns;
} triggered_event_t;

int sequencer_update(triggered_event_t *triggered_events, int max_events) {
    if (!atomic_load(&g_sequencer.running)) {
        return 0;
    }
    
    uint64_t now = get_accurate_time_ns();
    uint64_t start_time = atomic_load(&g_sequencer.start_time_ns);
    uint64_t elapsed_ns = now - start_time;
    
    int triggered_count = 0;
    
    // Check for triggered events
    for (int i = 0; i < MAX_EVENTS && triggered_count < max_events; i++) {
        if (g_events[i].active && elapsed_ns >= g_events[i].beat_time_ns) {
            // Event should trigger
            triggered_events[triggered_count].event_id = g_events[i].event_id;
            triggered_events[triggered_count].actual_time_ns = now;
            
            // Convert back to beat time for reporting
            uint32_t bpm_fixed = atomic_load(&g_sequencer.bpm_fixed);
            double bpm = bpm_fixed / 1000.0;
            triggered_events[triggered_count].beat_time = 
                (double)g_events[i].beat_time_ns * bpm / 60000000000.0;
            
            triggered_count++;
            
            // Remove one-shot event (or keep for looping events)
            g_events[i].active = false;
        }
    }
    
    // Update beat counter for display/sync purposes
    uint32_t bpm_fixed = atomic_load(&g_sequencer.bpm_fixed);
    double bpm = bpm_fixed / 1000.0;
    uint32_t current_beat = (uint32_t)(elapsed_ns * bpm / 60000000000.0);
    atomic_store(&g_sequencer.beat_count, current_beat);
    
    return triggered_count;
}

// Utility: Schedule events in a pattern (e.g., every quarter note)
void sequencer_schedule_pattern(double start_beat, double interval_beats, 
                               int event_id, int count) {
    for (int i = 0; i < count; i++) {
        sequencer_schedule_event(start_beat + i * interval_beats, event_id + i);
    }
}

// Utility: Get timing statistics
sequencer_stats_t sequencer_get_stats(void) {
    sequencer_stats_t stats = {0};
    
    stats.running = atomic_load(&g_sequencer.running);
    if (!stats.running) {
        return stats;
    }
    
    uint32_t bpm_fixed = atomic_load(&g_sequencer.bpm_fixed);
    stats.current_bpm = bpm_fixed / 1000.0;
    stats.current_beat_position = sequencer_get_beat_position();
    stats.beat_count = atomic_load(&g_sequencer.beat_count);
    
    uint64_t now = get_accurate_time_ns();
    uint64_t start_time = atomic_load(&g_sequencer.start_time_ns);
    stats.elapsed_time_ms = (now - start_time) / 1000000;
    
    return stats;
}

uint64_t find_next_event_time(void);

// Recommended: Adaptive sleep with lookahead
void timing_thread_loop(void) {
    triggered_event_t events[32];
    uint64_t lookahead_ns = 2000000;  // 2ms lookahead window
    uint64_t max_sleep_ns = 1000000;  // Max 1ms sleep
    uint64_t min_sleep_ns = 50000;    // Min 0.05ms sleep when events are near
    
    while (true) {
        uint64_t loop_start = get_accurate_time_ns();
        
        // Check for triggered events
        int count = sequencer_update(events, 32);
        
        // Process triggered events
        for (int i = 0; i < count; i++) {
            printf("Event %d triggered at beat %.3f\n", 
                   events[i].event_id, events[i].beat_time);
            // Handle your event here
        }
        
        // Adaptive sleep based on next event proximity
        uint64_t next_event_time = find_next_event_time();
        uint64_t now = get_accurate_time_ns();
        uint64_t sleep_ns;
        
        if (next_event_time == UINT64_MAX) {
            // No events scheduled - use maximum sleep
            sleep_ns = max_sleep_ns;
        } else if (next_event_time <= now + lookahead_ns) {
            // Event coming soon - use minimum sleep for precision
            sleep_ns = min_sleep_ns;
        } else {
            // Event is far away - sleep longer but not too long
            uint64_t time_to_event = next_event_time - now - lookahead_ns;
            sleep_ns = time_to_event / 4;  // Sleep 1/4 of remaining time
            
            // Clamp to min/max bounds
            if (sleep_ns > max_sleep_ns) sleep_ns = max_sleep_ns;
            if (sleep_ns < min_sleep_ns) sleep_ns = min_sleep_ns;
        }
        
        struct timespec sleep_time = {
            .tv_sec = sleep_ns / 1000000000,
            .tv_nsec = sleep_ns % 1000000000
        };
        nanosleep(&sleep_time, NULL);
    }
}

// Alternative: Event-driven approach with conditional variables
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint64_t next_wake_time_ns;
    bool shutdown;
} timing_thread_state_t;

static timing_thread_state_t g_timing_thread = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .next_wake_time_ns = 0,
    .shutdown = false
};

// More efficient: sleep until next event
void timing_thread_loop_efficient(void) {
    triggered_event_t events[32];
    
    while (!g_timing_thread.shutdown) {
        int count = sequencer_update(events, 32);
        
        // Process triggered events
        for (int i = 0; i < count; i++) {
            printf("Event %d triggered at beat %.3f\n", 
                   events[i].event_id, events[i].beat_time);
        }
        
        // Find next event time to optimize sleep duration
        uint64_t next_event_time = find_next_event_time();
        uint64_t now = get_accurate_time_ns();
        
        if (next_event_time > now) {
            uint64_t sleep_ns = next_event_time - now;
            
            // Cap sleep time to maintain responsiveness
            if (sleep_ns > 10000000) { // Max 10ms sleep
                sleep_ns = 10000000;
            }
            
            struct timespec sleep_time = {
                .tv_sec = sleep_ns / 1000000000,
                .tv_nsec = sleep_ns % 1000000000
            };
            nanosleep(&sleep_time, NULL);
        }
    }
}

// Helper function to find the next scheduled event
uint64_t find_next_event_time(void) {
    uint64_t next_time = UINT64_MAX;
    uint64_t start_time = atomic_load(&g_sequencer.start_time_ns);
    
    for (int i = 0; i < MAX_EVENTS; i++) {
        if (g_events[i].active) {
            uint64_t event_abs_time = start_time + g_events[i].beat_time_ns;
            if (event_abs_time < next_time) {
                next_time = event_abs_time;
            }
        }
    }
    
    return next_time;
}
