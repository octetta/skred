#include "skred.h"
#include "wire.h"
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
  tempo_base = m;
  tempo_bpm = m / 4.0;
  float bps = m / 60.f;
  float time_per_step = 1.0f / bps / 4.0f;
  //printf("# BPM %g -> BPS %g -> time_per_step %g\n", m, bps, time_per_step);
  tempo_time_per_step = time_per_step;
}

#if 0

// Assumptions: these exist elsewhere in your codebase:
// uint64_t synth_sample_count;            // global sample counter (samples)
// #define MAIN_SAMPLE_RATE 44100
// double tempo_time_per_step;             // seconds per step (set elsewhere)
// char seq_pattern[PATTERNS_MAX][SEQ_STEPS_MAX][STEP_MAX];
// int seq_pattern_mute[PATTERNS_MAX][SEQ_STEPS_MAX];
// int seq_pointer[PATTERNS_MAX]; etc.
// queued_t work_queue[QUEUE_SIZE];
// wire() / sparse() signatures as before
// WIRE() macro available

void seq(int frame_count) {
  // --- bounds & quick sanity
  if (frame_count <= 0) return;

  // Window for this callback: [start_sample, end_sample]
  uint64_t start_sample = synth_sample_count;
  uint64_t end_sample = synth_sample_count + (uint64_t)frame_count - 1ULL;

  // --- 1) Process work_queue items that fall inside this buffer window
  // (assumes work_queue[].when is expressed in samples)
  // local context per work item to avoid persistent mutation of a static `v`
  for (int q = 0; q < QUEUE_SIZE; q++) {
    if (work_queue[q].state == Q_READY) {
      uint64_t when = work_queue[q].when;
      if (when <= end_sample) { // scheduled inside this buffer or earlier
        work_queue[q].state = Q_USING;
        wire_t v_local = WIRE();
        v_local.voice = work_queue[q].voice;
        wire(work_queue[q].what, &v_local);
        work_queue[q].state = Q_FREE;
      }
    }
  }

  // --- 2) Prepare per-cell execution contexts (non-static, to avoid hidden state)
  // We'll create local contexts when firing a cell below.

  // --- 3) Timing: sample-accurate step scheduling
  static uint64_t samples_per_step = 0;
  static double last_tempo_time_per_step = 0.0;

  // recompute samples_per_step if tempo changed (cheap)
  if (tempo_time_per_step != last_tempo_time_per_step || samples_per_step == 0) {
    double t = tempo_time_per_step;
    if (t <= 0.0) t = 1.0 / 1000.0; // fallback tiny step to avoid zero
    uint64_t sps = (uint64_t)(t * (double)MAIN_SAMPLE_RATE + 0.5);
    if (sps == 0) sps = 1;
    samples_per_step = sps;
    last_tempo_time_per_step = tempo_time_per_step;
  }

  // We'll keep a static next_step_sample that denotes the sample index of the next step to fire.
  // Initialize next_step_sample to the upcoming step if it isn't set yet.
  static uint64_t next_step_sample = 0;
  static int next_step_inited = 0;
  if (!next_step_inited) {
    // schedule first step at the next sample boundary (you can adjust if you want an offset)
    next_step_sample = start_sample + samples_per_step;
    next_step_inited = 1;
  }

  // If tempo changed drastically, ensure next_step_sample remains >= start_sample to avoid
  // accidental firing of many steps in one callback due to a tempo jump.
  if (next_step_sample < start_sample) next_step_sample = start_sample + samples_per_step;

  // --- 4) Fire all steps that occur inside this callback window
  // (A single callback may include 0..N steps)
  while (next_step_sample <= end_sample) {
    // It's time to run one sequencer step at sample position next_step_sample.

    // For each pattern, run step logic
    for (int p = 0; p < PATTERNS_MAX; p++) {
      if (seq_state[p] != SEQ_RUNNING) continue;

      // modulo handling (skip this pattern's step if modulo requires it)
      if (seq_modulo[p] > 1) {
        if ((seq_counter[p] % seq_modulo[p]) != 0) {
          seq_counter[p]++;
          continue;
        }
      }
      seq_counter[p]++;

      // Safe step index handling
      int sp = seq_pointer[p];
      if (sp < 0) sp = 0;
      if (sp >= SEQ_STEPS_MAX) {
        // corruption defense: wrap to 0
        sp = 0;
        seq_pointer[p] = 0;
      }

      // read cell (fixed-length char array)
      char *cell = seq_pattern[p][sp];
      int empty = (cell[0] == '\0');

      if (!empty) {
        int muted = 0;
        if ((sp >= 0) && (sp < SEQ_STEPS_MAX)) muted = seq_pattern_mute[p][sp];

        if (!muted) {
          wire_t w_local = WIRE(); // fresh context per fired cell
          wire(cell, &w_local);
        }
      }

      // Advance the pointer safely and decide wrap
      int next_sp = sp + 1;
      if (next_sp >= SEQ_STEPS_MAX) {
        seq_pointer[p] = 0;
      } else {
        if (seq_pattern[p][next_sp][0] == '\0') seq_pointer[p] = 0;
        else seq_pointer[p] = next_sp;
      }
    } // end for each pattern

    // schedule the next sequencer step
    // (be careful if tempo changed drastically; this will step forward by samples_per_step)
    next_step_sample += samples_per_step;
  } // end while steps in this buffer

  // --- 5) Advance global sample counter AFTER processing this buffer
  // This keeps comparisons consistent: we used [start_sample, end_sample] above.
  synth_sample_count += (uint64_t)frame_count;
}



#else

void seq(int frame_count) {
  // static int voice = 0;
  // static voice_stack_t vs;
  // static int state = 0;
  
  // run expired (ready) queued things...
  static wire_t v = WIRE();
  for (int q = 0; q < QUEUE_SIZE; q++) {
    if ((work_queue[q].state == Q_READY) && (work_queue[q].when <= (synth_sample_count + frame_count))) {
      work_queue[q].state = Q_USING;
      v.voice = work_queue[q].voice;
      wire(work_queue[q].what, &v);
      work_queue[q].state = Q_FREE;
    }
  }

  static wire_t w = WIRE();

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
      if (seq_pattern_mute[p][seq_pointer[p]] == 0) wire(seq_pattern[p][seq_pointer[p]], &w);
      seq_pointer[p]++;
      switch (seq_pattern[p][seq_pointer[p]][0]) {
        case '\0':
          seq_pointer[p] = 0;
          break;
      }
    }
  }
}

#endif


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
  for (int p = 0; p < PATTERNS_MAX; p++) {
    pattern_reset(p);
 
  }
#if 0
  synth_sample_count = 0;
  seq_next_step_sample = 0;
  seq_samples_per_step = compute_samples_per_step(tempo_bpm, MAIN_SAMPLE_RATE);
#endif
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

