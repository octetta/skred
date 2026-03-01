#ifndef _SEQ_H_
#define _SEQ_H_

#define SEQ_FRAMES_PER_CALLBACK (128)

#define PATTERNS_MAX (16)
#define SEQ_STEPS_MAX (256)
#define STEP_MAX (256)

enum {
  SEQ_STOPPED = 0,
  SEQ_RUNNING = 1,
  SEQ_PAUSED = 2,
};

#define QUEUED_MAX (2048) // the wire string max size
#define QUEUE_SIZE (2048) // the actual queue max depth

typedef struct {
  int state;
  char what[QUEUED_MAX];
  int voice;
} event_t;

void seq(int frame_count, void (*queue_fn)(int voice, char *arg), void (*pattern_fn)(int voice, char *arg));
void seq_init(void);
void seq_rewind(void);
uint64_t seq_master_tick(void);
void pattern_reset(int p);
int queue_item(uint64_t when, char *what, int voice, int tag);
void tempo_set(float m);

void seq_modulo_set(int pattern, int m);
void seq_mute_set(int pattern, int step, int m);
void seq_step_set(int pattern, int step, char *scratch);
void seq_pattern_length_set(int pattern, int len);
void seq_step_goto(int pattern, int step);
void seq_state_set(int p, int state);
void seq_state_all(int state);

extern int requested_seq_frames_per_callback;
extern int seq_frames_per_callback;

extern int seq_pointer[PATTERNS_MAX];
extern int seq_modulo[PATTERNS_MAX];
extern int seq_counter[PATTERNS_MAX];
extern int seq_state[PATTERNS_MAX];
extern int seq_pattern_mute[PATTERNS_MAX][SEQ_STEPS_MAX];
extern int seq_pattern_length[PATTERNS_MAX];
extern char seq_pattern[PATTERNS_MAX][SEQ_STEPS_MAX][STEP_MAX];

extern float tempo_bpm;
extern float tempo_time_per_step;

int seq_queued(void);
int seq_capacity(void);

int seq_foreach(int (*fn)(int, uint64_t, uint64_t, int, const event_t *e, void*), void *user);

int seq_kill_by_tag(int tag);

char *seq_stats(void);

int seq_kill_all(void);

#endif
