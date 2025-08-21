#include "raylib.h"

int main(void) {
  const int screenWidth = 800;
  const int screenHeight = 450;
  InitWindow(screenWidth, screenHeight, "raylib [core] example - keyboard input");
  Vector2 ballPosition = { (float)screenWidth/2, (float)screenHeight/2 };
  SetTargetFPS(60);
  while (!WindowShouldClose()) {
    if (IsKeyDown(KEY_RIGHT)) ballPosition.x += 2.0f;
    if (IsKeyDown(KEY_LEFT)) ballPosition.x -= 2.0f;
    if (IsKeyDown(KEY_UP)) ballPosition.y -= 2.0f;
    if (IsKeyDown(KEY_DOWN)) ballPosition.y += 2.0f;
    BeginDrawing();
      ClearBackground(RAYWHITE);
      DrawText("move the ball with arrow keys", 10, 10, 20, DARKGRAY);
      DrawCircleV(ballPosition, 50, MAROON);
    EndDrawing();
  }
  CloseWindow();
  return 0;
}
