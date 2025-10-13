#ifndef _SEQ_H_
#define _SEQ_H_

void seq(int num_frames);
void seq_init(void);
void pattern_reset(int p);
int queue_item(uint64_t when, char *what, int voice);
void tempo_set(float m);

void seq_modulo_set(int pattern, int m);
void seq_mute_set(int pattern, int step, int m);
void seq_step_set(int pattern, int step, char *scratch);
void seq_state_set(int p, int state);
void seq_state_all(int state);

extern int requested_seq_frames_per_callback;
extern int seq_frames_per_callback;

extern int seq_pointer[PATTERNS_MAX];
extern int seq_modulo[PATTERNS_MAX];
extern int seq_counter[PATTERNS_MAX];
extern int seq_state[PATTERNS_MAX];
extern int seq_pattern_mute[PATTERNS_MAX][SEQ_STEPS_MAX];
extern char seq_pattern[PATTERNS_MAX][SEQ_STEPS_MAX][STEP_MAX];

extern queued_t work_queue[QUEUE_SIZE];

#endif
