#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>

#include "raylib.h"
#include "rlgl.h"

int debug = 1;
int trace = 0;

#define MAIN_SAMPLE_RATE (44100)

#define SCREENWIDTH (800)
#define SCREENHEIGHT (480)

#define SCOPE_LEN (MAIN_SAMPLE_RATE/7)

float tick_max = 10.0f;
float tick_inc = 1.5f;
int tick_frames = 2048;
uint64_t sample_count = 0;
int console_voice = 0;

char *voice_format(int n, char *s) {
  sprintf(s, "v%d", n);
  return s;
}

enum {
  SCOPE_TRIGGER_BOTH,
  SCOPE_TRIGGER_RISING,
  SCOPE_TRIGGER_FALLING,
};

int scope_trigger_mode = SCOPE_TRIGGER_RISING;

int scope_running = 0;
int scope_len = SCOPE_LEN;
float scope_buffer_left[SCOPE_LEN];
float scope_buffer_right[SCOPE_LEN];
int scope_buffer_pointer = 0;
float scope_display_pointer = 0.0f;
float scope_display_inc = 1.0f;
float scope_display_mag = 1.0f;
#define SCOPE_WAVE_WIDTH (SCREENWIDTH/4)
#define SCOPE_WAVE_HEIGHT (SCREENHEIGHT/2)
float scope_wave_data[SCOPE_WAVE_WIDTH];
int scope_wave_index = 0;
float scope_wave_min[SCOPE_WAVE_WIDTH];
float scope_wave_max[SCOPE_WAVE_WIDTH];
int scope_wave_len = 0;
int scope_channel = -1; // -1 means all

int find_starting_point(int buffer_snap, int sw) {
  int j = buffer_snap - sw;
  float t0 = (scope_buffer_left[j] + scope_buffer_right[j]) / 2.0f;
  int i = j-1;
  int c = 1;
  while (c < SCOPE_LEN) {
    if (i < 0) i = SCOPE_LEN-1;
    float t1 = (scope_buffer_left[i] + scope_buffer_right[i]) / 2.0f;
    if (t0 == 0.0 && t1 == 0.0) {
      // zero
    } else if (t0 < 0 && t1 > 0) break;
    i--;
    c++;
    t0 = t1;
  }
  return i;
}

#define MAG_X_INC (0.05)

#define CONFIG_FILE ".skred_window"

pthread_t scope_thread_handle;

void *scope_main(void *arg) {
  pthread_setname_np(pthread_self(), "skred-scope");
  //
  Vector2 position_in;
  FILE *file = fopen(CONFIG_FILE, "r");
  const int screenWidth = SCREENWIDTH;
  const int screenHeight = SCREENHEIGHT;
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
    // if (mag_x <= 0) mag_x = 1;
    fclose(file);
  } else {
    printf("# %s read fopen fail\n", CONFIG_FILE);
  }
  //
  //SetTraceLogLevel(LOG_NONE);
  InitWindow(screenWidth, screenHeight, "skred-scope");
  //
  SetWindowPosition((int)position_in.x, (int)position_in.y);
  //
  Vector2 dot = { (float)screenWidth/2, (float)screenHeight/2 };
  SetTargetFPS(12);
  float sh = (float)screenHeight;
  float h0 = (float)screenHeight / 2.0f;
  char osd[1024] = "?";
  int osd_dirty = 1;
  float y = h0;
  float a = 1.0f;
  int show_l = 1;
  int show_r = 1;
  Color color_left = {0, 255, 255, 128};
  Color color_right = {255, 255, 0, 128};
  Color green0 = {0, 255, 0, 128};
  Color green1 = {0, 128, 0, 128};
  while (scope_running && !WindowShouldClose()) {
    if (mag_x <= 0) mag_x = MAG_X_INC;
    sw = (float)screenWidth / mag_x;
    float mw = (float)SCOPE_LEN / 2.0f;
    if (sw >= mw) sw = mw;
    int shifted = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyDown(KEY_ONE)) {
      if (show_l == 1) show_l = 0; else show_l = 1;
    }
    if (IsKeyDown(KEY_TWO)) {
      if (show_r == 1) show_r = 0; else show_r = 1;
    }
    if (IsKeyDown(KEY_A)) {
      if (shifted) {
        scope_display_mag -= 0.1f;
        a -= 0.1f;
      } else {
        scope_display_mag += 0.1f;
        a += 0.1f;
      }
      osd_dirty++;
    }
    if (IsKeyDown(KEY_RIGHT)) {
      mag_x += (float)MAG_X_INC;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_LEFT)) {
      mag_x -= (float)MAG_X_INC;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_UP)) {
      y += 1.0f;
      osd_dirty++;
    }
    if (IsKeyDown(KEY_DOWN)) {
      y -= 1.0f;
      osd_dirty++;
    }
    if (osd_dirty) {
      sprintf(osd, "mag_x:%g sw:%g y:%g a:%g mag:%g ch:%d",
        mag_x, sw, y, a, scope_display_mag, scope_channel);
    }
    int buffer_snap = scope_buffer_pointer;
    BeginDrawing();
    ClearBackground(BLACK);
    // show a wave table
    if (scope_wave_len) {
      for (int i = 0; i < SCOPE_WAVE_WIDTH; i++) {
        int mh = SCOPE_WAVE_HEIGHT / 2;
        float min = (scope_wave_min[i] / 2.0f + 0.5f) * (float)mh;
        float max = (scope_wave_max[i] / 2.0f + 0.5f) * (float)mh;
        int qh = SCOPE_WAVE_HEIGHT / 4;
        DrawLine(i, (int)max, i, qh, green1);
        DrawLine(i, (int)min, i, qh, green1);
        dot.x = (float)i;
        dot.y = max; DrawCircleV(dot, 1, green0);
        dot.y = min; DrawCircleV(dot, 1, green0);
      }
      char s[32];
      sprintf(s, "W%d", scope_wave_index);
      DrawText(s, 10, 10, 20, YELLOW);
    }
    // find starting point for display
    int start = find_starting_point(buffer_snap, screenWidth);
    int actual = 0;
    float x_offset = (float)SCREENWIDTH / 8.0f;
    start -= (int)x_offset;
    DrawText(osd, 10, SCREENHEIGHT-20, 20, GREEN);
    rlPushMatrix();
    rlTranslatef(0.0f, y, 0.0f); // x,y,z
    rlScalef(mag_x,scope_display_mag,1.0f);
      DrawLine(0, 0, (int)sw, 0, DARKGREEN);
      if (start >= scope_len) start = 0;
      actual = start;
      for (int i = 0; i < (int)sw; i++) {
        if (actual == 0) DrawLine(i, (int)-sh, i, (int)sh, YELLOW);
        if (actual == buffer_snap) DrawLine(i, (int)-sh, i, (int)sh, BLUE);
        if (actual >= (SCOPE_LEN-1)) {
          DrawLine(i, (int)-sh, i, (int)sh, RED);
          actual = 0;
        }
        dot.x = (float)i;
        if (show_l) {
          dot.y = scope_buffer_left[actual] * h0 * scope_display_mag;
          DrawCircleV(dot, 1, color_right);
        }
        if (show_r) {
          dot.y = scope_buffer_right[actual] * h0 * scope_display_mag;
          DrawCircleV(dot, 1, color_left);
        }
        actual++;
      }
    rlPopMatrix();
    sprintf(osd, "M%g,%g %d:%ld", tick_max, tick_inc, tick_frames, sample_count);
    DrawText(osd, SCREENWIDTH-250, SCREENHEIGHT-20, 20, BLUE);
    voice_format(console_voice, osd);
    DrawText(osd, 10, SCREENHEIGHT-40, 20, YELLOW);
    EndDrawing();
  }
  //
  file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    Vector2 position_out = GetWindowPosition();
    fprintf(file, "%g %g %g %g", position_out.x, position_out.y, scope_display_mag, mag_x);
    fclose(file);
  } else {
    printf("# %s write fopen fail\n", CONFIG_FILE);
  }
  //
  CloseWindow();
  scope_running = 0;
  if (debug) printf("# scope stopping\n");
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
  scope_wave_len = SCOPE_WAVE_WIDTH;
  scope_start(0);
  float sine0;
  float sine1;
  float phase0 = 0;
  float phase1 = 0;
  float delta0 = 1.0f / 256.0f;
  float delta1 = 1.0f / 333.2f;
  int i = 0;
  while (1) {
    sine0 = sinf(2.0f * (float)M_PI * phase0);
    sine1 = sinf(2.0f * (float)M_PI * phase1);
    scope_buffer_left[i] = sine0;
    scope_buffer_right[i] = sine1;
    usleep(1);
    i++;
    phase0 += delta0;
    phase1 += delta1;
    if (i >= SCOPE_LEN) i = 0;
    sample_count++;
  }
  return 0;
}
