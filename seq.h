#ifndef _SEQ_H_
#define _SEQ_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    double current_bpm;
    double current_beat_position;
    uint32_t beat_count;
    uint64_t elapsed_time_ms;
    bool running;
} sequencer_stats_t;

void inside_audio_callback(void);
int sequencer_schedule_event(double beat_time, int event_id);
void timing_thread_loop(void);
void sequencer_start(double bpm);
void sequencer_stop(void);
void sequencer_set_bpm(double bpm);
double sequencer_get_beat_position(void);
int sequencer_schedule_event(double beat_time, int event_id);
void sequencer_remove_event(int slot_index);
sequencer_stats_t sequencer_get_stats(void);

#endif
