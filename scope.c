#include "raylib.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>
#else
typedef unsigned long DWORD;
extern __declspec(dllimport) void __stdcall Sleep(DWORD dwMilliseconds);
#endif

#include "scope-shared.h"
#include "skred-mem.h"

static scope_buffer_t safety;
static scope_buffer_t *scope = &safety;

typedef enum { TRIGGER_NONE = 0, TRIGGER_ZERO_RISING, TRIGGER_ZERO_RISING_HYST } scope_trigger_t;
scope_trigger_t scope_trigger_mode = TRIGGER_ZERO_RISING_HYST;
int scope_running = 0;
float scope_display_mag = 1.0f;
bool persistence_enabled = false;
int persistence_alpha = 40; 
bool trigger_locked = false;
bool is_small_mode = false;
bool restart_requested = false;

// Optimization state
static int last_audio_frame = -1;

// --- STRUCT DEFINITIONS FIRST ---
typedef struct { int index; float offset; } trigger_result_t;
static trigger_result_t cached_trig = {0};

float get_buf_avg(int i) {
    int len = scope->buffer_len;
    if (len <= 0) return 0;
    int idx = (i + len) % len;
    return (scope->buffer_left[idx] + scope->buffer_right[idx]) * 0.5f;
}

trigger_result_t find_trigger(int write_ptr, int window) {
    int len = scope->buffer_len;
    trigger_result_t res = { (write_ptr - window + len) % len, 0.0f };
    trigger_locked = false;
    if (scope_trigger_mode == TRIGGER_NONE) return res;
    const float HYST_LOW = -0.08f, HYST_HIGH = 0.08f;
    int search_depth = len / 2;
    int i = (write_ptr - 64 + len) % len; 
    float v_next = get_buf_avg(i);
    for (int c = 0; c < search_depth; c++) {
        int i_cur = (i - 1 + len) % len;
        float v_cur = get_buf_avg(i_cur);
        if (scope_trigger_mode == TRIGGER_ZERO_RISING_HYST) {
            if (v_cur < HYST_LOW && v_next > HYST_HIGH) {
                res.index = i_cur;
                float diff = v_next - v_cur;
                if (diff > 0.0001f) res.offset = (0.0f - v_cur) / diff;
                trigger_locked = true; return res;
            }
        } else if (scope_trigger_mode == TRIGGER_ZERO_RISING) {
            if (v_cur <= 0.0f && v_next > 0.0f) {
                res.index = i_cur;
                trigger_locked = true; return res;
            }
        }
        v_next = v_cur; i = i_cur;
    }
    return res; 
}

#define MAG_X_INC (0.05)
#define CONFIG_FILE ".skred_window"

void scope_run_loop() {
    float mag_x = 1.0f;
    Vector2 pos_in = {100, 100};
    restart_requested = false;

    FILE *f = fopen(CONFIG_FILE, "r");
    if (f) {
        fscanf(f, "%f %f %f %f", &pos_in.x, &pos_in.y, &scope_display_mag, &mag_x);
        fclose(f);
    }

    int winW = is_small_mode ? SCOPE_WIDTH_IN_PIXELS / 2 : SCOPE_WIDTH_IN_PIXELS;
    int winH = is_small_mode ? SCOPE_HEIGHT_IN_PIXELS / 2 : SCOPE_HEIGHT_IN_PIXELS;

    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(winW, winH, is_small_mode ? "Skred-o-Scope 2 (Small)" : "Skred-o-Scope 2 [HiDPI]");
    SetWindowPosition((int)pos_in.x, (int)pos_in.y);
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(SCOPE_WIDTH_IN_PIXELS, SCOPE_HEIGHT_IN_PIXELS);
    int display_frame_count = 0;

    while (!WindowShouldClose() && !restart_requested) {
        // --- CPU SAVER: Check for new data ---
        bool data_is_new = (scope->frame_count != last_audio_frame);
        
        // Handle input even if no new data (allows smooth zooming/gain even when audio stops)
        if (IsKeyPressed(KEY_S)) { is_small_mode = !is_small_mode; restart_requested = true; break; }
        if (IsKeyPressed(KEY_P)) persistence_enabled = !persistence_enabled;
        if (IsKeyDown(KEY_UP)) persistence_alpha = (int)fmin(255, persistence_alpha + 2);
        if (IsKeyDown(KEY_DOWN)) persistence_alpha = (int)fmax(5, persistence_alpha - 2);
        if (IsKeyPressed(KEY_ONE)) scope_display_mag += 0.1f;
        if (IsKeyPressed(KEY_TWO)) scope_display_mag = fmaxf(0.1f, scope_display_mag - 0.1f);
        if (IsKeyPressed(KEY_RIGHT)) mag_x += (float)MAG_X_INC;
        if (IsKeyPressed(KEY_LEFT)) mag_x = fmaxf((float)MAG_X_INC, mag_x - (float)MAG_X_INC);
        if (IsKeyPressed(KEY_Z)) scope_trigger_mode = TRIGGER_ZERO_RISING;
        if (IsKeyPressed(KEY_X)) scope_trigger_mode = TRIGGER_ZERO_RISING_HYST;
        if (IsKeyPressed(KEY_B)) scope_trigger_mode = TRIGGER_NONE;

        // Only do heavy waveform drawing if there is new data OR we need persistence fades
        if (data_is_new || persistence_enabled) {
            float sw_samples = (float)SCOPE_WIDTH_IN_PIXELS / (mag_x > 0 ? mag_x : 1.0f);
            
            if (data_is_new) {
                cached_trig = find_trigger(scope->buffer_pointer, (int)sw_samples);
                last_audio_frame = scope->frame_count;
                display_frame_count = scope->frame_count;
            }

            BeginTextureMode(target);
            if (persistence_enabled) DrawRectangle(0, 0, SCOPE_WIDTH_IN_PIXELS, SCOPE_HEIGHT_IN_PIXELS, (Color){0, 0, 0, (unsigned char)persistence_alpha});
            else ClearBackground(BLACK);

            rlPushMatrix();
            rlTranslatef(0, (float)SCOPE_HEIGHT_IN_PIXELS/2.0f, 0); 
            rlScalef((float)SCOPE_WIDTH_IN_PIXELS / sw_samples, 1.0f, 1.0f);
            rlTranslatef(-cached_trig.offset, 0, 0);

            rlSetBlendMode(RL_BLEND_ADDITIVE);
            float vScale = (SCOPE_HEIGHT_IN_PIXELS / 2.0f) * scope_display_mag;
            Color color_l = (Color){ 255, 255, 0, 255 }; // Yellow
            Color color_r = (Color){ 0, 255, 255, 255 }; // Cyan

            for (int i = 0; i < (int)sw_samples; i++) {
                int idx = (cached_trig.index + i) % scope->buffer_len;
                DrawPixelV((Vector2){(float)i, scope->buffer_left[idx] * vScale}, color_l);
                DrawPixelV((Vector2){(float)i, scope->buffer_right[idx] * vScale}, color_r);
            }
            rlSetBlendMode(RL_BLEND_ALPHA);
            rlPopMatrix();
            EndTextureMode();
        } else {
            // No new data, persistence off. Give CPU back to OS for ~8ms.
            #ifdef _WIN32
                Sleep(8);
            #else
                usleep(8000);
            #endif
        }

        BeginDrawing();
        ClearBackground(BLACK);
        float curW = (float)GetScreenWidth();
        float curH = (float)GetScreenHeight();
        float uiScale = curH / (float)SCOPE_HEIGHT_IN_PIXELS;

        // Draw the cached texture (cheap)
        DrawTexturePro(target.texture, (Rectangle){0, 0, (float)target.texture.width, (float)-target.texture.height},
            (Rectangle){0, 0, curW, curH}, (Vector2){0,0}, 0, WHITE);

        // --- UI Layer (Static or simple overlays) ---
        if (scope->wave_len) {
            float mid_y = (SCOPE_WAVE_HEIGHT / 4.0f) * uiScale;
            for (int i = 0; i < SCOPE_WAVE_WIDTH; i++) {
                float x = (float)i * uiScale;
                float min_y = (scope->wave_min[i] / 2.0f + 0.5f) * (SCOPE_WAVE_HEIGHT / 2.0f) * uiScale;
                float max_y = (scope->wave_max[i] / 2.0f + 0.5f) * (SCOPE_WAVE_HEIGHT / 2.0f) * uiScale;
                DrawLine(x, (int)max_y, x, (int)mid_y, (Color){0, 255, 0, 100});
                DrawLine(x, (int)min_y, x, (int)mid_y, (Color){0, 255, 0, 100});
                DrawCircle(x, (int)max_y, 1, GREEN);
                DrawCircle(x, (int)min_y, 1, GREEN);
            }
            DrawText(scope->wave_text, 10, 10, 20 * uiScale, YELLOW);
        }

        const char* trig_name = (scope_trigger_mode == TRIGGER_NONE) ? "AUTO" : (scope_trigger_mode == TRIGGER_ZERO_RISING) ? "ZERO" : "HYST";
        float boxW = 150 * uiScale;
        float boxH = 85 * uiScale;
        DrawRectangle(curW - boxW - 10, 10, boxW, boxH, (Color){0, 0, 0, 180});
        DrawText(TextFormat("TRIG: %s", trig_name), curW - boxW, 15 * uiScale, 18 * uiScale, RAYWHITE);
        DrawText(trigger_locked ? "LOCK: YES" : "LOCK: NO", curW - boxW, 35 * uiScale, 18 * uiScale, trigger_locked ? GREEN : RED);
        DrawText(TextFormat("P-FADE: %d", persistence_alpha), curW - boxW, 55 * uiScale, 18 * uiScale, GOLD);
        DrawText(TextFormat("FB: %d", display_frame_count), curW - boxW, 75 * uiScale, 14 * uiScale, DARKGREEN);

        DrawText(scope->status_text, 10, curH - (25 * uiScale), 20 * uiScale, BLUE);
        DrawText(scope->voice_text, 10, curH - (50 * uiScale), 20 * uiScale, YELLOW);
        DrawText(scope->debug_text, 10, curH - (75 * uiScale), 20 * uiScale, RED);
        EndDrawing();
    }

    f = fopen(CONFIG_FILE, "w");
    if (f) {
        Vector2 p = GetWindowPosition();
        fprintf(f, "%g %g %g %g", p.x, p.y, scope_display_mag, mag_x);
        fclose(f);
    }
    UnloadRenderTexture(target);
    CloseWindow();
    if (!restart_requested) scope_running = 0;
}

int main(int argc, char *argv[]) {
    skred_mem_t *xyz = skred_mem_new();
    if (skred_mem_open(xyz, "skred-o-scope.001", sizeof(scope_buffer_t)) != 0) exit(1);
    scope = (scope_buffer_t *)skred_mem_addr(xyz);
    scope_running = 1;
    while (scope_running) scope_run_loop();
    skred_mem_close(xyz); 
    return 0;
}