#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "scope-shared.h"

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
int scope_len = SCOPE_WIDTH_IN_SAMPLES;
float scope_display_pointer = 0.0f;
float scope_display_inc = 1.0f;
float scope_display_mag = 1.0f;

int find_starting_point(int buffer_snap, int sw) {
  int j = buffer_snap - sw;
  float t0 = (scope->buffer_left[j] + scope->buffer_right[j]) / 2.0f;
  int i = j-1;
  int c = 1;
  while (c < SCOPE_WIDTH_IN_SAMPLES) {
    if (i < 0) i = SCOPE_WIDTH_IN_SAMPLES-1;
    float t1 = (scope->buffer_left[i] + scope->buffer_right[i]) / 2.0f;
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
  const int screenWidth = SCOPE_WIDTH_IN_PIXELS;
  const int screenHeight = SCOPE_HEIGHT_IN_PIXELS;
  float mag_x = 1.0f;
  float sw = (float)screenWidth;
  int fps = 10;
  int last_fps = fps;
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
    //printf("# %s read fopen fail\n", CONFIG_FILE);
  }
  //
  SetTraceLogLevel(LOG_NONE);
  InitWindow(screenWidth, screenHeight, "skred-scope");
  //
  SetWindowPosition((int)position_in.x, (int)position_in.y);
  //
  Vector2 dot = { (float)screenWidth/2, (float)screenHeight/2 };
  SetTargetFPS(fps);
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
    float mw = (float)SCOPE_WIDTH_IN_SAMPLES / 2.0f;
    if (sw >= mw) sw = mw;
    int shifted = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_ZERO)) fps--;
    if (IsKeyPressed(KEY_NINE)) fps++;
    if (fps <= 0) fps = 1;
    if (fps > 60) fps = 60;
    if (fps != last_fps) {
      osd_dirty++;
      SetTargetFPS(fps);
      last_fps = fps;
    }
    if (IsKeyDown(KEY_ONE)) {
      if (show_l == 1) show_l = 0; else show_l = 1;
    }
    if (IsKeyDown(KEY_TWO)) {
      if (show_r == 1) show_r = 0; else show_r = 1;
    }
    if (IsKeyPressed(KEY_A)) {
      if (shifted) {
        scope_display_mag -= 0.1f;
        a -= 0.1f;
      } else {
        scope_display_mag += 0.1f;
        a += 0.1f;
      }
      osd_dirty++;
    }
    if (IsKeyPressed(KEY_RIGHT)) {
      mag_x += (float)MAG_X_INC;
      osd_dirty++;
    }
    if (IsKeyPressed(KEY_LEFT)) {
      mag_x -= (float)MAG_X_INC;
      osd_dirty++;
    }
    if (IsKeyPressed(KEY_UP)) {
      y += 1.0f;
      osd_dirty++;
    }
    if (IsKeyPressed(KEY_DOWN)) {
      y -= 1.0f;
      osd_dirty++;
    }
    if (osd_dirty) {
      sprintf(osd, "%g,%g %g,%g %d",
        mag_x, sw, a, scope_display_mag, fps);
    }
    int buffer_snap = scope->buffer_pointer;
    BeginDrawing();
    ClearBackground(BLACK);
    // show a wave table
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
    // find starting point for display
    int start = find_starting_point(buffer_snap, screenWidth);
    int actual = 0;
    float x_offset = (float)SCOPE_WIDTH_IN_PIXELS / 8.0f;
    start -= (int)x_offset;
    DrawText(osd, 10, SCOPE_HEIGHT_IN_PIXELS-20, 20, GREEN);
    rlPushMatrix();
    rlTranslatef(0.0f, y, 0.0f); // x,y,z
    rlScalef(mag_x,scope_display_mag,1.0f);
      DrawLine(0, 0, (int)sw, 0, DARKGREEN);
      if (start >= scope_len) start = 0;
      actual = start;
      for (int i = 0; i < (int)sw; i++) {
        if (actual == 0) DrawLine(i, (int)-sh, i, (int)sh, YELLOW);
        if (actual == buffer_snap) DrawLine(i, (int)-sh, i, (int)sh, BLUE);
        if (actual >= (SCOPE_WIDTH_IN_SAMPLES-1)) {
          DrawLine(i, (int)-sh, i, (int)sh, RED);
          actual = 0;
        }
        dot.x = (float)i;
        if (show_l) {
          dot.y = scope->buffer_left[actual] * h0 * scope_display_mag;
          DrawCircleV(dot, 1, color_right);
        }
        if (show_r) {
          dot.y = scope->buffer_right[actual] * h0 * scope_display_mag;
          DrawCircleV(dot, 1, color_left);
        }
        actual++;
      }
    rlPopMatrix();
    DrawText(scope->status_text, SCOPE_WIDTH_IN_PIXELS-250, SCOPE_HEIGHT_IN_PIXELS-20, 20, BLUE);
    DrawText(scope->voice_text, 10, SCOPE_HEIGHT_IN_PIXELS-40, 20, YELLOW);
    EndDrawing();
  }
  //
  file = fopen(CONFIG_FILE, "w");
  if (file != NULL) {
    Vector2 position_out = GetWindowPosition();
    fprintf(file, "%g %g %g %g", position_out.x, position_out.y, scope_display_mag, mag_x);
    fclose(file);
  } else {
    //printf("# %s write fopen fail\n", CONFIG_FILE);
  }
  //
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
  scope_share_t shared;
  scope = scope_setup(&shared, "r");
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
