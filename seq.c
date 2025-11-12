#include "skred.h"
#ifndef SKODE
#include "wire.h"
#else
#include "skode.h"
#endif
#include "synth-types.h"
#include "synth.h"
#include "seq.h"

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

void tempo_set(float m) {
  tempo_bpm = m;
  float bps = m / 60.f;
  float time_per_step = 1.0f / bps;
  //printf("# BPM %g -> BPS %g -> time_per_step %g\n", m, bps, time_per_step);
  tempo_time_per_step = time_per_step;
}

void seq(int frame_count) {
  // static int voice = 0;
  // static voice_stack_t vs;
  // static int state = 0;
  
  // run expired (ready) queued things...
#ifndef SKODE
  static wire_t v = WIRE();
#else
#endif
  for (int q = 0; q < QUEUE_SIZE; q++) {
    if ((work_queue[q].state == Q_READY) && (work_queue[q].when < synth_sample_count)) {
      work_queue[q].state = Q_USING;
#ifndef SKODE
      v.voice = work_queue[q].voice;
      wire(work_queue[q].what, &v);
#else
#endif
      work_queue[q].state = Q_FREE;
    }
  }

#ifndef SKODE
  static wire_t w = WIRE();
#else
  static int skode_first = 1;
  static skode_t w;
  if (skode_first) {
    skode_first = 0;
    sparse_init(&w, NULL); 
  }
  
#endif

  int advance = 0;
  static float clock_sec = 0.0f;
  float frame_time_sec = (float)frame_count / (float)MAIN_SAMPLE_RATE;
  clock_sec += frame_time_sec;
  if (clock_sec >= tempo_time_per_step) {
    advance = 1; // trigger next step
    clock_sec -= tempo_time_per_step;
  } else {
    advance = 0;
  }

  if (advance) {
#if 0
    // SIGH... poor little scope needs fixing...
    sprintf(new_scope->debug_text, "%d:%d %s",
      scope_pattern_pointer, seq_pointer[scope_pattern_pointer],
      seq_pattern[scope_pattern_pointer][seq_pointer[scope_pattern_pointer]]);
#endif
    for (int p = 0; p < PATTERNS_MAX; p++) {
      if (seq_state[p] != SEQ_RUNNING) continue;
      if (seq_modulo[p] > 1) {
        if ((seq_counter[p] % seq_modulo[p]) != 0) {
          seq_counter[p]++;
          continue;
        }
      }
      seq_counter[p]++;
#ifndef SKODE
      if (seq_pattern_mute[p][seq_pointer[p]] == 0) wire(seq_pattern[p][seq_pointer[p]], &w);
#else
      if (seq_pattern_mute[p][seq_pointer[p]] == 0) {
        char *s = seq_pattern[p][seq_pointer[p]];
        sparse(&w, s, strlen(s));
      }
#endif
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
  seq_modulo[p] = 1;
  for (int s = 0; s < SEQ_STEPS_MAX; s++) {
    seq_pattern[p][s][0] = '\0';
    seq_pattern_mute[p][s] = 0;
  }
}

void seq_init(void) {
  for (int p = 0; p < PATTERNS_MAX; p++) {
    pattern_reset(p);
  }
}

queued_t work_queue[QUEUE_SIZE];

int queue_item(uint64_t when, char *what, int voice) {
  int p = -1;
  for (int q = 0; q < QUEUE_SIZE; q++) {
    if (work_queue[q].state == Q_FREE) {
      work_queue[q].state = Q_PREP;
      work_queue[q].when = when;
      work_queue[q].voice = voice;
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

