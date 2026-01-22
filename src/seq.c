#include "synth-types.h"
#include "synth.h"
#include "seq.h"

#include "skqueue.h"

#include <stdio.h>
#include <string.h>

int requested_seq_frames_per_callback = SEQ_FRAMES_PER_CALLBACK;
int seq_frames_per_callback = 0;

char seq_pattern[PATTERNS_MAX][SEQ_STEPS_MAX][STEP_MAX];
int seq_pattern_mute[PATTERNS_MAX][SEQ_STEPS_MAX];

int scope_pattern_pointer = 0;
int seq_pointer[PATTERNS_MAX];
int seq_counter[PATTERNS_MAX];
int seq_state[PATTERNS_MAX];
int seq_modulo[PATTERNS_MAX];

float tempo_time_per_step = 60.0f;
float tempo_bpm = 120.0f / 4.0f;
float tempo_base = 0.0f;

void tempo_set(float m) {
  tempo_base = m;
  tempo_bpm = m / 4.0;
  float bps = m / 60.f;
  float time_per_step = 1.0f / bps / 4.0f;
  //printf("# BPM %g -> BPS %g -> time_per_step %g\n", m, bps, time_per_step);
  tempo_time_per_step = time_per_step;
}

static queue_t seq_q;

void seq(int frame_count, void (*queue_fn)(int voice, char *arg), void (*pattern_fn)(int voice, char *arg)) {
  // run expired (ready) queued things...
  item_t item;
  uint64_t now = synth_sample_count + frame_count; // not sure about adding frame count here but it's below from before?
  while (queue_get_filtered(&seq_q, now, &item)) {
    int q = (int)(intptr_t)item.data;
    queue_fn(work_queue[q].voice, work_queue[q].what);
    work_queue[q].state = Q_FREE;
  }
#if 0
  // old queue stuff
  for (int q = 0; q < QUEUE_SIZE; q++) {
    if ((work_queue[q].state == Q_READY) && (work_queue[q].when <= (synth_sample_count + frame_count))) {
      work_queue[q].state = Q_USING;
      queue_fn(work_queue[q].voice, work_queue[q].what);
      work_queue[q].state = Q_FREE;
    }
  }
#endif

  int advance = 0;
  static double clock_sec = 0.0f;
  float frame_time_sec = (float)frame_count / (float)MAIN_SAMPLE_RATE;
  clock_sec += frame_time_sec;
  if (clock_sec >= tempo_time_per_step) {
    advance = 1; // trigger next step
    clock_sec -= tempo_time_per_step;
  } else {
    advance = 0;
  }

  if (advance) {

    for (int p = 0; p < PATTERNS_MAX; p++) {
      if (seq_state[p] != SEQ_RUNNING) continue;
      if (seq_modulo[p] > 1) {
        if ((seq_counter[p] % seq_modulo[p]) != 0) {
          seq_counter[p]++;
          continue;
        }
      }
      seq_counter[p]++;
      if (seq_pattern_mute[p][seq_pointer[p]] == 0) {
        pattern_fn(0, seq_pattern[p][seq_pointer[p]]);
      }
      seq_pointer[p]++;
      switch (seq_pattern[p][seq_pointer[p]][0]) {
        case '\0':
          seq_pointer[p] = 0;
          break;
      }
    }
  }
}

void pattern_reset(int p) {
  seq_pointer[p] = 0;
  seq_state[p] = SEQ_STOPPED;
  seq_counter[p] = 0;
  seq_modulo[p] = 4;
  for (int s = 0; s < SEQ_STEPS_MAX; s++) {
    seq_pattern[p][s][0] = '\0';
    seq_pattern_mute[p][s] = 0;
  }
}

queued_t work_queue[QUEUE_SIZE];

static int free_stack[QUEUE_SIZE];
static int free_top = -1;
static nsync_mu pool_mu;

static void pool_init(void) {
  nsync_mu_init(&pool_mu);
  for (int i = 0; i < QUEUE_SIZE; i++) {
    free_stack[++free_top] = i;
  }
}

void seq_init(void) {
  pool_init();
  queue_init(&seq_q, QUEUE_SIZE);
  for (int p = 0; p < PATTERNS_MAX; p++) {
    pattern_reset(p);
  }
}

int queue_item(uint64_t when, char *what, int voice, int tag) {
  nsync_mu_lock(&pool_mu);
  if (free_top < 0) {
    nsync_mu_unlock(&pool_mu);
    return 0; // pool full... this WRONG since i want to start over-writing older
  }

  int q = free_stack[free_top--];
  nsync_mu_unlock(&pool_mu);

  work_queue[q].state = Q_PREP;
  work_queue[q].when = when;
  work_queue[q].voice = voice;
  work_queue[q].tag = tag;
  strcpy(work_queue[q].what, what);
  work_queue[q].state = Q_READY;

  queue_put(&seq_q, when, tag, (void*)(intptr_t)q);
  
  return 0;

  // old queue
  int p = -1;
  for (int q = 0; q < QUEUE_SIZE; q++) {
    if (work_queue[q].state == Q_FREE) {
      work_queue[q].state = Q_PREP;
      work_queue[q].when = when;
      work_queue[q].voice = voice;
      work_queue[q].tag = tag;
      strcpy(work_queue[q].what, what);
      work_queue[q].state = Q_READY;
      p = q;
      break;
    }
  }
  return p;
}

void seq_modulo_set(int pattern, int m) {
  seq_modulo[pattern] = m;
}

void seq_mute_set(int pattern, int step, int m) {
  seq_pattern_mute[pattern][step] = m;
}

void seq_step_set(int pattern, int step, char *scratch) {
  if (strlen(scratch) == 0) seq_pattern[pattern][step][0] = '\0';
  strcpy(seq_pattern[pattern][step], scratch);
}


void seq_state_set(int p, int state) {
  switch (state) {
    case 0: // stop
      seq_state[p] = SEQ_STOPPED;
      seq_pointer[p] = 0;
      break;
    case 1: // start
      seq_state[p] = SEQ_RUNNING;
      seq_pointer[p] = 0;
      break;
    case 2: // pause
      seq_state[p] = SEQ_PAUSED;
      break;
    case 3: // resume
      seq_state[p] = SEQ_RUNNING;
      break;
  }
}

void seq_state_all(int state) {
  for (int p = 0; p < PATTERNS_MAX; p++) seq_state_set(p, state);
}

