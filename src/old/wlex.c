// example usage of winline

#include "winline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char *line;
  if (winlineInit() < 0) {
    fprintf(stderr, "Failed to initialize winline\n");
    return 1;
  }
  winlineHistoryLoad("history.txt");
  winlineHistorySetMaxLen(100);
  printf("Winline example. Type 'help' for commands, 'quit' to exit.\n");
  while (1) {
    line = winlineReadLine("winline> ");
    if (line == NULL) {
      // EOF (Ctrl+C or error)
      break;
    }
    if (line[0] == '\0') {
      free(line);
      continue;
    }
    winlineHistoryAdd(line);
    if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
      free(line);
      break;
    } else if (strcmp(line, "help") == 0) {
      printf("Commands:\n");
      printf("  help  - Show this help\n");
      printf("  clear - Clear screen\n");
      printf("  quit  - Exit program\n");
      printf("\nEditing:\n");
      printf("  Arrow keys   - Navigate line and history\n");
      printf("  Home/End   - Move to start/end of line\n");
      printf("  Backspace  - Delete character before cursor\n");
      printf("  Delete     - Delete character at cursor\n");
      printf("  Escape     - Clear line\n");
    } else if (strcmp(line, "clear") == 0) {
      winlineClearScreen();
    } else {
      printf("You entered: %s\n", line);
    }
    free(line);
  }
  winlineHistorySave("history.txt");
  winlineCleanup();
  printf("\nGoodbye!\n");
  return 0;
}
