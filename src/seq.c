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

void seq(int frame_count, void (*event_fn)(int voice, char *arg), void (*pattern_fn)(int voice, char *arg)) {
  // run expired (ready) queued things...
  item_t item;
  uint64_t now = synth_sample_count + frame_count; // not sure about adding frame count here but it's below from before?
  while (queue_get_filtered(&seq_q, now, &item)) {
    event_fn(item.event.voice, item.event.what);
  }

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

void seq_init(void) {
  queue_init(&seq_q, QUEUE_SIZE);
  for (int p = 0; p < PATTERNS_MAX; p++) {
    pattern_reset(p);
  }
}

int queue_item(uint64_t when, char *what, int voice, int tag) {
  queue_put(&seq_q, when, tag, NULL, voice, what);
  return 0;
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

int seq_queued(void) { return queue_size(&seq_q); }
int seq_capacity(void) { return seq_q.max_size; }

typedef struct {
  const int (*fn)(int, uint64_t, uint64_t, int, const event_t *e, void*);
  void *user;
} bridge_t;

int seq_foreach_cb(const item_t *item, void *user) {
  bridge_t *b = (bridge_t *)user;
  b->fn(666, item->timestamp, item->id, item->tag, &item->event, b->user);
  printf("%ld %ld %d %s\n",
    item->timestamp, item->id, item->tag,
    item->event.what);
  return 0;
}

int seq_foreach(int (*fn)(int, uint64_t, uint64_t, int, const event_t *e, void*), void *user) {
#if 1
  bridge_t b;
  b.fn = fn;
  b.user = user;
  queue_foreach(&seq_q, seq_foreach_cb, &b);
  return 0;
#else
  bridge_t a;
  int n = seq_q.size;
  for (int i=0; i<n; i++) {
    int r = 0;
    if (fn) {
      r = fn(i, seq_q.items[i].timestamp, seq_q.items[i].id,
        seq_q.items[i].tag, &seq_q.items[i].event, user);
    }
  }
  return n;
#endif
}

bool kill_by_tag(const item_t *item, void *user) {
  int *tag = (int *)user;
  if (item->tag == *tag) return true;
  return false;
}

int seq_kill_by_tag(int tag) {
  queue_cancel(&seq_q, kill_by_tag, &tag);
  return 0;
}
