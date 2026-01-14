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

#define QUEUED_MAX (1024)
#define QUEUE_SIZE (1024)

enum {
  Q_FREE = 0,
  Q_PREP = 1,
  Q_READY = 2,
  Q_USING = 3,
};

typedef struct {
  int state;
  uint64_t when;
  char what[QUEUED_MAX];
  int voice;
} queued_t;

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

extern float tempo_bpm;
extern float tempo_time_per_step;

extern queued_t work_queue[QUEUE_SIZE];

#endif
