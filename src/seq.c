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
int seq_pattern_length[PATTERNS_MAX];

int scope_pattern_pointer = 0;
int seq_pointer[PATTERNS_MAX];  // read-only derived value, kept for external inspection
int seq_counter[PATTERNS_MAX];  // kept for external inspection / compat
int seq_state[PATTERNS_MAX];
int seq_modulo[PATTERNS_MAX];

// Per-pattern offset applied to the master clock when deriving step position:
//   step = ((master_tick / modulo) - seq_offset[p]) % length
// Normally zero. seq_step_goto() writes this to force a specific step next tick.
static int64_t seq_offset[PATTERNS_MAX];

// Master clock tick — ever-incrementing, never reset unless seq_rewind() is called.
// All pattern positions are derived from this value so stop/start is always phase-coherent.
static uint64_t master_tick = 0;

float tempo_time_per_step = 60.0f;
float tempo_bpm = 120.0f / 4.0f;
float tempo_base = 0.0f;

void tempo_set(float m) {
  tempo_base = m;
  tempo_bpm = m / 4.0;
  float bps = m / 60.f;
  float time_per_step = 1.0f / bps / 4.0f;
  tempo_time_per_step = time_per_step;
}

void seq_rewind(void) {
  master_tick = 0;
}

uint64_t seq_master_tick(void) {
  return master_tick;
}

#include "util.h"

static sben_t bench[BENLEN] = {};
static int benchp = 0;
static int64_t bencho = 0;
static char _stats[65536] = "";

char *seq_stats(void) {
  char *ptr = _stats;
  *ptr = '\0';
  int n = 0;
  for (int i = 0; i < BENLEN; i++) {
    if (bench[i].state != BEN_B) continue;
    double dms = ts_diff_ns(&bench[i].a, &bench[i].b) / (double)NS_TO_MS;
    n = sprintf(ptr, "# @%d %gms\n", bench[i].order, dms);
    ptr += n;
    bench[i].state = BEN_0;
  }
  return _stats;
}

static int pattern_length_compute(int p) {
  for (int s = SEQ_STEPS_MAX - 1; s >= 0; s--) {
    if (seq_pattern[p][s][0] != '\0') return s + 1;
  }
  return 0;
}

static queue_t seq_q;

void seq(int frame_count, void (*event_fn)(int voice, char *arg), void (*pattern_fn)(int voice, char *arg)) {
  BEN_MARK_A(bench, benchp, frame_count, bencho);

  // Run expired (ready) queued events.
  item_t item;
  uint64_t now = SAMPLE_COUNT_GET();
  static uint64_t last_ts = 0;
  while (1) {
    if (queue_get_filtered(&seq_q, now, &item)) {
      if (item.timestamp < last_ts) {
        printf("OUT OF ORDER\n");
      }
      last_ts = item.timestamp;
      event_fn(item.event.voice, item.event.what);
    } else {
      break;
    }
  }

  // Advance the master clock one tick when enough time has elapsed.
  static double clock_sec = 0.0;
  float frame_time_sec = (float)frame_count / (float)MAIN_SAMPLE_RATE;
  clock_sec += frame_time_sec;

  int advance = 0;
  if (clock_sec >= tempo_time_per_step) {
    advance = 1;
    clock_sec -= tempo_time_per_step;
    master_tick++;
  }

  if (advance) {
    for (int p = 0; p < PATTERNS_MAX; p++) {
      if (seq_state[p] != SEQ_RUNNING) continue;

      if ((master_tick % (uint64_t)seq_modulo[p]) != 0) continue;

      int len = seq_pattern_length[p];
      if (len == 0) continue;

      int64_t ticks_so_far = (int64_t)(master_tick / (uint64_t)seq_modulo[p]);
      int64_t raw = ticks_so_far - seq_offset[p];
      int step = (int)(((raw % (int64_t)len) + (int64_t)len) % (int64_t)len);

      seq_pointer[p] = step;
      seq_counter[p]++;

      if (seq_pattern_mute[p][step] == 0) {
        pattern_fn(0, seq_pattern[p][step]);
      }
    }
  }

  BEN_MARK_B(bench, benchp, bencho);
}

void pattern_reset(int p) {
  seq_pointer[p] = 0;
  seq_state[p] = SEQ_STOPPED;
  seq_counter[p] = 0;
  seq_modulo[p] = 4;
  seq_pattern_length[p] = 0;
  seq_offset[p] = 0;
  for (int s = 0; s < SEQ_STEPS_MAX; s++) {
    seq_pattern[p][s][0] = '\0';
    seq_pattern_mute[p][s] = 0;
  }
}

void seq_init(void) {
  queue_init(&seq_q, QUEUE_SIZE);
  master_tick = 0;
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
  if (strlen(scratch) == 0) {
    seq_pattern[pattern][step][0] = '\0';
  } else {
    strcpy(seq_pattern[pattern][step], scratch);
  }
  seq_pattern_length[pattern] = pattern_length_compute(pattern);
}

void seq_pattern_length_set(int pattern, int len) {
  if (len < 0) len = 0;
  if (len > SEQ_STEPS_MAX) len = SEQ_STEPS_MAX;
  seq_pattern_length[pattern] = len;
}

// Jump to a specific step on the next tick. Adjusts seq_offset so that
// (ticks_so_far + 1 - offset) % len == step.
void seq_step_goto(int pattern, int step) {
  int len = seq_pattern_length[pattern];
  if (len == 0) return;
  if (step < 0 || step >= len) return;
  int64_t ticks_so_far = (int64_t)(master_tick / (uint64_t)seq_modulo[pattern]);
  seq_offset[pattern] = (ticks_so_far + 1) - (int64_t)step;
}

void seq_state_set(int p, int state) {
  switch (state) {
    case 0: // stop
      seq_state[p] = SEQ_STOPPED;
      break;
    case 1: // start
      seq_state[p] = SEQ_RUNNING;
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
  int (*fn)(int, uint64_t, uint64_t, int, const event_t *e, void*);
  void *user;
} bridge_t;

int seq_foreach_cb(const item_t *item, void *user) {
  bridge_t *b = (bridge_t *)user;
  b->fn(666, item->timestamp, item->id, item->tag, &item->event, b->user);
  return 0;
}

int seq_foreach(int (*fn)(int, uint64_t, uint64_t, int, const event_t *e, void*), void *user) {
  bridge_t b;
  b.fn = fn;
  b.user = user;
  queue_foreach(&seq_q, seq_foreach_cb, &b);
  return 0;
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

int seq_kill_all(void) {
  queue_clear(&seq_q);
  return 0;
}
