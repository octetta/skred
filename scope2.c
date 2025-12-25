#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "scope-shared.h"
#include "skred-mem.h"

static scope_buffer_t safety;
static scope_buffer_t *scope = &safety;

#include "raylib.h"
#include "rlgl.h"

enum {
  SCOPE_TRIGGER_BOTH,
  SCOPE_TRIGGER_RISING,
  SCOPE_TRIGGER_FALLING,
};

int scope_trigger_mode = SCOPE_TRIGGER_RISING;

int scope_running = 0;
int scope_width = SCOPE_WIDTH_IN_SAMPLES;
float scope_display_pointer = 0.0f;
float scope_display_inc = 1.0f;
float scope_display_mag = 1.0f;

int find_start(int buffer_pointer, int sw) {
  int j = buffer_pointer - sw;
  float t0 = (scope->buffer_left[j] + scope->buffer_right[j]) / 2.0f;
  int i = j-1;
  int c = 1;
  int cc = 0;
  while (c < sw) {
    if (i < 0) i = sw-1;
    float t1 = (scope->buffer_left[i] + scope->buffer_right[i]) / 2.0f;
    if (t0 == 0.0 && t1 == 0.0) {
      // zero
    } else if (t0 < 0 && t1 > 0) {
      cc++;
      if (cc > 1) break;
    }
    i--;
    c++;
    t0 = t1;
  }
  return i;
}

#define MAG_X_INC (0.05)

#define CONFIG_FILE ".skred_window"

pthread_t scope_thread_handle;

#include "futex.h"

static long futex_wait_timeout(volatile uint32_t *uaddr, uint32_t expected, int timeout_ms) {
  struct timespec ts;

  // Get current monotonic time
  clock_gettime(CLOCK_MONOTONIC, &ts);

  // Add timeout (convert ms to seconds + nanoseconds)
  ts.tv_sec  += timeout_ms / 1000;
  ts.tv_nsec += (timeout_ms % 1000) * 1000000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000;
  }

  return syscall(SYS_futex, uaddr, FUTEX_WAIT_PRIVATE, expected, &ts);
}

void *scope_main(void *arg) {
  pthread_setname_np(pthread_self(), "skred-o-scope-2");
  //
  Vector2 position_in;
  FILE *file = fopen(CONFIG_FILE, "r");
  const int screenWidth = SCOPE_WIDTH_IN_PIXELS;
  const int screenHeight = SCOPE_HEIGHT_IN_PIXELS;
  float mag_x = 1.0f;
  float sw = (float)screenWidth;
  if (file != NULL) {
    /*
     file contents
    x y scope_display_mag mag_x
    */
    fscanf(file, "%f %f %f %f", &position_in.x, &position_in.y, &scope_display_mag, &mag_x);
    if (position_in.x < 0) position_in.x = 0;
    if (position_in.y < 0) position_in.y = 0;
    if (scope_display_mag <= 0) scope_display_mag = 1;
    fclose(file);
  } else {
    //printf("# %s read fopen fail\n", CONFIG_FILE);
  }
  //
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  SetTraceLogLevel(LOG_NONE);
  InitWindow(screenWidth, screenHeight, "skred-o-scope-2");
  //
  SetWindowPosition((int)position_in.x, (int)position_in.y);
  //
  Vector2 dot = { (float)screenWidth/2, (float)screenHeight/2 };
  SetTargetFPS(60); // Set reasonable target FPS instead of unlimited
  float sh = (float)screenHeight;
  float h0 = (float)screenHeight / 2.0f;
  char osd[1024] = "?";
  float y = h0;
  float a = 1.0f;
  int show_l = 1;
  int show_r = 1;
  Color color_left = {0, 255, 255, 128};
  Color color_right = {255, 255, 0, 128};
  Color green0 = {0, 255, 0, 128};
  Color green1 = {0, 128, 0, 128};
  int osd_dirty = 0;
  int last_frame_count = -1;
  int frames_without_update = 0;
  const int STALE_THRESHOLD = 180; // 3 seconds at 60fps - assume synth died
  
  while (scope_running && !WindowShouldClose()) {
    // Always process input first to keep window responsive
    if (mag_x <= 0) mag_x = MAG_X_INC;
    sw = (float)screenWidth / mag_x;
    float mw = (float)SCOPE_WIDTH_IN_SAMPLES / 2.0f;
    if (sw >= mw) sw = mw;
    
    if (IsKeyPressed(KEY_ONE)) show_l = !show_l;
    if (IsKeyPressed(KEY_TWO)) show_r = !show_r;
    if (IsKeyPressed(KEY_RIGHT)) mag_x += (float)MAG_X_INC;
    if (IsKeyPressed(KEY_LEFT)) mag_x -= (float)MAG_X_INC;
    
    int shifted = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_A)) {
      if (shifted) {
        scope_display_mag -= 0.1f;
        a -= 0.1f;
      } else {
        scope_display_mag += 0.1f;
        a += 0.1f;
      }
    }
    
    // Check for new frame (non-blocking)
    volatile uint32_t *futex_word = (volatile uint32_t *)&scope->frame_count;
    int current_count = __atomic_load_n(futex_word, __ATOMIC_ACQUIRE);
    
    // Detect synth restart: if we've been stale a while, reset tracking
    if (frames_without_update > STALE_THRESHOLD) {
      last_frame_count = current_count - 1; // Force next check to see it as new
      frames_without_update = 0;
      sprintf(osd, "synth reconnected (count=%d)", current_count);
    }
    
    // Check if this is a new frame
    bool has_new_data = (current_count != last_frame_count);
    
    if (has_new_data) {
      last_frame_count = current_count;
      frames_without_update = 0;
      sprintf(osd, "frame %d", current_count);
    } else {
      frames_without_update++;
      if (frames_without_update == 60) {
        sprintf(osd, "waiting for synth...");
      }
      // Don't block on futex here - just poll and let raylib's FPS limiter handle timing
    }
    
    if (osd_dirty) {
      sprintf(osd, "%d %d", scope->frame_count, scope->buffer_pointer);
    }
    
    int buffer_pointer = scope->buffer_pointer;
    
    BeginDrawing();
    ClearBackground(BLACK);
    
    // Show wave table
    if (scope->wave_len) {
      for (int i = 0; i < SCOPE_WAVE_WIDTH; i++) {
        int mh = SCOPE_WAVE_HEIGHT / 2;
        float min = (scope->wave_min[i] / 2.0f + 0.5f) * (float)mh;
        float max = (scope->wave_max[i] / 2.0f + 0.5f) * (float)mh;
        int qh = SCOPE_WAVE_HEIGHT / 4;
        DrawLine(i, (int)max, i, qh, green1);
        DrawLine(i, (int)min, i, qh, green1);
        dot.x = (float)i;
        dot.y = max; DrawCircleV(dot, 1, green0);
        dot.y = min; DrawCircleV(dot, 1, green0);
      }
      DrawText(scope->wave_text, 10, 10, 20, YELLOW);
    }
    
    // Find starting point for display
    int start = find_start(buffer_pointer, screenWidth);
    float x_offset = (float)SCOPE_WIDTH_IN_PIXELS / 8.0f;
    start -= (int)x_offset;
    
    DrawText(osd, 10, SCOPE_HEIGHT_IN_PIXELS-20, 20, GREEN);
    
    rlPushMatrix();
    rlTranslatef(0.0f, y, 0.0f);
    rlScalef(mag_x, scope_display_mag, 1.0f);
    
    DrawLine(0, 0, (int)sw, 0, DARKGREEN);
    
    start = start % scope_width;
    int actual = start;
    
    // Draw dots as small circles - batch by channel for better performance
    if (show_l) {
      for (int i = 0; i < (int)sw; i++) {
        dot.x = (float)i;
        dot.y = scope->buffer_left[actual] * h0;
        DrawCircleV(dot, 1, color_right);
        actual = (actual + 1) % scope_width;
      }
    }
    
    actual = start;
    if (show_r) {
      for (int i = 0; i < (int)sw; i++) {
        dot.x = (float)i;
        dot.y = scope->buffer_right[actual] * h0;
        DrawCircleV(dot, 1, color_left);
        actual = (actual + 1) % scope_width;
      }
    }
    
    rlPopMatrix();
    
    DrawText(scope->status_text, SCOPE_WIDTH_IN_PIXELS-250, SCOPE_HEIGHT_IN_PIXELS-20, 20, BLUE);
    DrawText(scope->voice_text, 10, SCOPE_HEIGHT_IN_PIXELS-40, 20, YELLOW);
    DrawText(scope->debug_text, 10, SCOPE_HEIGHT_IN_PIXELS-60, 20, RED);
    
    EndDrawing();
  }
  
  // Save config
  file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    Vector2 position_out = GetWindowPosition();
    fprintf(file, "%g %g %g %g", position_out.x, position_out.y, scope_display_mag, mag_x);
    fclose(file);
  }
  
  CloseWindow();
  scope_running = 0;
  sleep(5);
  return NULL;
}

int scope_start(int sub) {
  if (scope_running == 0) {
    scope_running = 1;
    pthread_create(&scope_thread_handle, NULL, scope_main, NULL);
    pthread_detach(scope_thread_handle);
  }
  return 0;
}

int main(int argc, char *argv[]) {
  pid_t pid = fork();
  if (pid < 0) {
    exit(1);
  }
  if (pid > 0) {
    exit(0);
  }
  if (setsid() < 0) {
    exit(2);
  }
  skred_mem_t xyz;
  if (skred_mem_open(&xyz, "skred-o-scope.001", sizeof(scope_buffer_t)) != 0) {
    printf("# fail\n");
    exit(1);
  }
  scope = (scope_buffer_t *)xyz.addr;
  if (scope == NULL) {
    printf("# can't attach to shared scope buffer\n");
    exit(3);
  }
  // ugly but safe for now
  scope->wave_text[0] = '\0';
  scope->status_text[0] = '\0';
  scope->voice_text[0] = '\0';
  scope_start(0);
  sleep(1);
  while (scope_running) {
    sleep(1);
  }
  return 0;
}
