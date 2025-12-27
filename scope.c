#include "raylib.h"
#include "rlgl.h"
#include <errno.h>
#include <math.h>
#ifndef _WIN32
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#else
// Minimal Windows declarations - avoid including windows.h to prevent conflicts
typedef unsigned long DWORD;
extern __declspec(dllimport) DWORD __stdcall GetLastError(void);
extern __declspec(dllimport) void __stdcall Sleep(DWORD dwMilliseconds);
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "scope-shared.h"

static scope_buffer_t safety;
static scope_buffer_t *scope = &safety;

#include "skred-mem.h"

// enum {
//   SCOPE_TRIGGER_BOTH,
//   SCOPE_TRIGGER_RISING,
//   SCOPE_TRIGGER_FALLING,
// };

// int scope_trigger_mode = SCOPE_TRIGGER_RISING;


typedef enum {
    TRIGGER_NONE = 0,          // no trigger, return write pointer
    TRIGGER_ZERO_RISING,       // simple zero crossing
    TRIGGER_ZERO_RISING_HYST,  // zero crossing with hysteresis
    TRIGGER_ZERO_SLOPE,        // hysteresis + slope
    TRIGGER_PEAK,              // align to local positive peak
} scope_trigger_t;

scope_trigger_t scope_trigger_mode = TRIGGER_ZERO_RISING;


int scope_running = 0;
int scope_width = SCOPE_WIDTH_IN_SAMPLES;
float scope_display_pointer = 0.0f;
float scope_display_inc = 1.0f;
float scope_display_mag = 1.0f;

float get_buffer_left(int i) {
  //return scope->buffer_left[i];
  if (i >= 0 && i < scope->buffer_len) return scope->buffer_left[i];
  return 0;
}

float get_buffer_right(int i) {
  //return scope->buffer_right[i];
  if (i >= 0 && i < scope->buffer_len) return scope->buffer_right[i];
  return 0;
}

float get_buffer_average(int j) {
  return (get_buffer_left(j) + get_buffer_right(j)) / 2.0f;
}

// int find_start(int buffer_pointer, int sw) {
//   int j = buffer_pointer - sw;
//   float t0 = get_buffer_average(j);
//   int i = j-1;
//   int c = 1;
//   int cc = 0;
//   while (c < sw) {
//     if (i < 0) i = sw-1;
//     float t1 = get_buffer_average(j);
//     if (t0 == 0.0 && t1 == 0.0) {
//       // zero
//     } else if (t0 < 0 && t1 > 0) {
//       cc++;
//       if (cc > 1) break;
//     }
//     i--;
//     c++;
//     t0 = t1;
//   }
//   return i;
// }

int find_start_triggered(int write_ptr, int window, scope_trigger_t mode)
{
    int len = scope->buffer_len;
    if (len <= 0) return write_ptr;

    /* search back up to ~2 screen widths */
    int max_search = window * 2;
    if (max_search > len) max_search = len;

    int i = write_ptr;
    float prev = get_buffer_average(i);

    /* parameters (tweakable) */
    const float ZERO_EPS      = 0.0f;
    const float HYST_LOW      = -0.02f;
    const float HYST_HIGH     =  0.02f;
    const float MIN_LEVEL     =  0.05f;
    const float MIN_SLOPE     =  0.01f;

    float best_peak = 0.0f;
    int   best_i    = write_ptr;

    for (int c = 0; c < max_search; c++) {
        i = (i - 1 + len) % len;
        float cur = get_buffer_average(i);
        float slope = cur - prev;

        switch (mode) {

        case TRIGGER_ZERO_RISING:
            if (prev <= ZERO_EPS && cur > ZERO_EPS)
                return i;
            break;

        case TRIGGER_ZERO_RISING_HYST:
            if (prev < HYST_LOW && cur > HYST_HIGH &&
                fabsf(cur) > MIN_LEVEL)
                return i;
            break;

        case TRIGGER_ZERO_SLOPE:
            if (prev < HYST_LOW && cur > HYST_HIGH &&
                slope > MIN_SLOPE &&
                fabsf(cur) > MIN_LEVEL)
                return i;
            break;

        case TRIGGER_PEAK:
            /* track best positive peak */
            if (cur > best_peak && cur > MIN_LEVEL) {
                best_peak = cur;
                best_i = i;
            }
            break;

        case TRIGGER_NONE:
        default:
            return write_ptr;
        }

        prev = cur;
    }

    if (mode == TRIGGER_PEAK && best_peak > 0.0f)
        return best_i;

    return write_ptr;
}


#define MAG_X_INC (0.05)

#define CONFIG_FILE ".skred_window"

#ifndef _WIN32
pthread_t scope_thread_handle;
#endif

void *scope_main(void *arg) {
#ifndef _WIN32
  pthread_setname_np(pthread_self(), "skred-o-scope-2");
#endif
  printf("scope_main started\n");
  fflush(stdout);
  
  Vector2 position_in = {100, 100};
  FILE *file = fopen(CONFIG_FILE, "r");
  const int screenWidth = SCOPE_WIDTH_IN_PIXELS;
  const int screenHeight = SCOPE_HEIGHT_IN_PIXELS;
  float mag_x = 1.0f;
  float sw = (float)screenWidth;
  if (file != NULL) {
    fscanf(file, "%f %f %f %f", &position_in.x, &position_in.y, &scope_display_mag, &mag_x);
    if (position_in.x < 0) position_in.x = 100;
    if (position_in.y < 0) position_in.y = 100;
    if (scope_display_mag <= 0) scope_display_mag = 1;
    fclose(file);
  } else {
    printf("# %s read fopen fail\n", CONFIG_FILE);
  }
  
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  SetTraceLogLevel(LOG_NONE);
  InitWindow(screenWidth, screenHeight, "skred-o-scope-2");
  SetWindowPosition((int)position_in.x, (int)position_in.y);
  
  Vector2 dot = { (float)screenWidth/2, (float)screenHeight/2 };
  SetTargetFPS(60);
  float sh = (float)screenHeight;
  float h0 = (float)screenHeight / 2.0f;
  char osd[1024] = "?";
  int osd_color = 0;
  float y = h0;
  float a = 1.0f;
  int show_l = 1;
  int show_r = 1;
  Color color_left = {0, 255, 255, 128};
  Color color_right = {255, 255, 0, 128};
  Color green0 = {0, 255, 0, 128};
  Color green1 = {0, 128, 0, 128};
  int last_frame_count = -1;
  int frames_without_update = 0;
  const int STALE_THRESHOLD = 180;
  
  // printf("scope_main entering loop\n");
  // fflush(stdout);
  
  while (scope_running && !WindowShouldClose()) {
    if (mag_x <= 0) mag_x = MAG_X_INC;
    sw = (float)screenWidth / mag_x;
    float mw = (float)SCOPE_WIDTH_IN_SAMPLES / 2.0f;
    if (sw >= mw) sw = mw;
    
    if (IsKeyPressed(KEY_ONE)) show_l = !show_l;
    if (IsKeyPressed(KEY_TWO)) show_r = !show_r;
    if (IsKeyPressed(KEY_RIGHT)) mag_x += (float)MAG_X_INC;
    if (IsKeyPressed(KEY_LEFT)) mag_x -= (float)MAG_X_INC;

    if (IsKeyPressed(KEY_Z)) scope_trigger_mode = TRIGGER_ZERO_RISING;
    if (IsKeyPressed(KEY_X)) scope_trigger_mode = TRIGGER_ZERO_RISING_HYST;
    if (IsKeyPressed(KEY_C)) scope_trigger_mode = TRIGGER_ZERO_SLOPE;
    if (IsKeyPressed(KEY_V)) scope_trigger_mode = TRIGGER_PEAK;
    if (IsKeyPressed(KEY_B)) scope_trigger_mode = TRIGGER_NONE;

    
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
    
    int current_count = scope->frame_count;
    
    if (frames_without_update > STALE_THRESHOLD) {
      last_frame_count = current_count - 1;
      frames_without_update = 0;
      sprintf(osd, "synth reconnected (count=%d)", current_count);
      osd_color = 2;
    }
    
    bool has_new_data = (current_count != last_frame_count);
    
    if (has_new_data) {
      last_frame_count = current_count;
      frames_without_update = 0;
      sprintf(osd, "frame %d", current_count);
      osd_color = 0;
    } else {
      frames_without_update++;
      if (frames_without_update == 60) {
        sprintf(osd, "waiting for synth...");
        osd_color = 1;
      }
    }
    
    int buffer_pointer = scope->buffer_pointer;
    
    BeginDrawing();
    ClearBackground(BLACK);
    
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
    
    // int start = find_start(buffer_pointer, screenWidth);

    int start = find_start_triggered(
        buffer_pointer,
        screenWidth,
        scope_trigger_mode
    );


    float x_offset = (float)SCOPE_WIDTH_IN_PIXELS / 8.0f;
    start -= (int)x_offset;
    
    switch (osd_color) {
      case 2:
        DrawText(osd, 10, SCOPE_HEIGHT_IN_PIXELS-20, 20, BLUE);
        break;
      case 1:
        DrawText(osd, 10, SCOPE_HEIGHT_IN_PIXELS-20, 20, RED);
        break;
      case 0:
      default:
        DrawText(osd, 10, SCOPE_HEIGHT_IN_PIXELS-20, 20, GREEN);
        break;
    }
    
    rlPushMatrix();
    rlTranslatef(0.0f, y, 0.0f);
    rlScalef(mag_x, scope_display_mag, 1.0f);
    
    DrawLine(0, 0, (int)sw, 0, DARKGREEN);
    
    start = start % scope_width;
    int actual = start;
    
    if (show_l) {
      for (int i = 0; i < (int)sw; i++) {
        dot.x = (float)i;
        dot.y = get_buffer_left(actual) * h0;
        DrawCircleV(dot, 1, color_right);
        actual = (actual + 1) % scope_width;
      }
    }
    
    actual = start;
    if (show_r) {
      for (int i = 0; i < (int)sw; i++) {
        dot.x = (float)i;
        dot.y = get_buffer_right(actual) * h0;
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
  
  printf("scope_main exited loop\n");
  fflush(stdout);
  
  file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    Vector2 position_out = GetWindowPosition();
    fprintf(file, "%g %g %g %g", position_out.x, position_out.y, scope_display_mag, mag_x);
    fclose(file);
  }
  
  CloseWindow();
  // printf("scope_main closed window\n");
  // fflush(stdout);
  
  scope_running = 0;
  
#ifndef _WIN32
  sleep(5);
#endif
  
  // printf("scope_main returning\n");
  // fflush(stdout);
  return NULL;
}

int scope_start(int sub) {
  if (scope_running == 0) {
    scope_running = 1;
#ifndef _WIN32
    pthread_create(&scope_thread_handle, NULL, scope_main, NULL);
    pthread_detach(scope_thread_handle);
#else
    // On Windows, just call directly (blocking but simpler)
    scope_main(NULL);
#endif
  }
  return 0;
}

int main(int argc, char *argv[]) {
#ifndef _WIN32
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
#else
  // printf("main: starting\n");
  // fflush(stdout);
#endif

  skred_mem_t *xyz = skred_mem_new();
  int r = skred_mem_open(xyz, "skred-o-scope.001", sizeof(scope_buffer_t));
  if (r != 0) {
#ifdef _WIN32
    DWORD err = GetLastError();
    printf("# fail (%d/%lu)\n", r, err);
#else
    printf("# fail (%d)\n", r);
#endif
    exit(1);
  }
  
  scope = (scope_buffer_t *)skred_mem_addr(xyz);
  if (scope == NULL) {
    printf("# can't attach to shared scope buffer\n");
    exit(3);
  }
  
  scope->wave_text[0] = '\0';
  scope->status_text[0] = '\0';
  scope->voice_text[0] = '\0';
  
  printf("main: calling scope_start\n");
  fflush(stdout);
  
  scope_start(0);
  
  printf("main: scope_start returned\n");
  fflush(stdout);
  
#ifndef _WIN32
  sleep(2);
  while (scope_running) {
    // printf("main/scope_running\n");
    // fflush(stdout);
    sleep(2);
  }
#endif
  
  // printf("main: cleaning up\n");
  // fflush(stdout);
  
  skred_mem_close(xyz);
  return 0;
}
