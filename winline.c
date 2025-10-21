/* winline.c - Implementation of minimal line editing for Windows */

#include "winline.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINLINE_MAX_LINE 4096
#define WINLINE_DEFAULT_HISTORY_MAX 100

typedef struct {
  char **lines;
  int count;
  int max;
} History;

static History history = {NULL, 0, WINLINE_DEFAULT_HISTORY_MAX};
static HANDLE hStdin = INVALID_HANDLE_VALUE;
static HANDLE hStdout = INVALID_HANDLE_VALUE;
static DWORD oldMode = 0;

int winlineInit(void) {
  hStdin = GetStdHandle(STD_INPUT_HANDLE);
  hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  
  if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
    return -1;
  }
  
  if (!GetConsoleMode(hStdin, &oldMode)) {
    return -1;
  }
  
  return 0;
}

void winlineCleanup(void) {
  if (hStdin != INVALID_HANDLE_VALUE) {
    SetConsoleMode(hStdin, oldMode);
  }
  
  for (int i = 0; i < history.count; i++) {
    free(history.lines[i]);
  }
  free(history.lines);
  history.lines = NULL;
  history.count = 0;
}

static void refreshLine(const char *prompt, const char *buf, int len, int pos) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  COORD coord;
  DWORD written;
  
  /* Get current cursor position */
  GetConsoleScreenBufferInfo(hStdout, &csbi);
  coord.X = 0;
  coord.Y = csbi.dwCursorPosition.Y;
  
  /* Clear the line */
  SetConsoleCursorPosition(hStdout, coord);
  FillConsoleOutputCharacter(hStdout, ' ', csbi.dwSize.X, coord, &written);
  SetConsoleCursorPosition(hStdout, coord);
  
  /* Write prompt and buffer */
  WriteConsole(hStdout, prompt, (DWORD)strlen(prompt), &written, NULL);
  WriteConsole(hStdout, buf, len, &written, NULL);
  
  /* Position cursor */
  coord.X = (SHORT)(strlen(prompt) + pos);
  SetConsoleCursorPosition(hStdout, coord);
}

char *winlineReadLine(const char *prompt) {
  char buf[WINLINE_MAX_LINE];
  int len = 0;
  int pos = 0;
  int historyIndex = history.count;
  char *saved = NULL;
  DWORD written;
  
  if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
    if (winlineInit() < 0) {
      return NULL;
    }
  }
  
  /* Set console mode for character input */
  SetConsoleMode(hStdin, ENABLE_PROCESSED_INPUT);
  
  /* Display prompt */
  WriteConsole(hStdout, prompt, (DWORD)strlen(prompt), &written, NULL);
  
  buf[0] = '\0';
  
  while (1) {
    INPUT_RECORD record;
    DWORD numRead;
    
    if (!ReadConsoleInput(hStdin, &record, 1, &numRead)) {
      break;
    }
    
    if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) {
      continue;
    }
    
    KEY_EVENT_RECORD *key = &record.Event.KeyEvent;
    
    switch (key->wVirtualKeyCode) {
      case VK_RETURN:
        WriteConsole(hStdout, "\r\n", 2, &written, NULL);
        buf[len] = '\0';
        if (saved) free(saved);
        return strdup(buf);
        
      case VK_BACK:
        if (pos > 0) {
          memmove(buf + pos - 1, buf + pos, len - pos);
          pos--;
          len--;
          buf[len] = '\0';
          refreshLine(prompt, buf, len, pos);
        }
        break;
        
      case VK_DELETE:
        if (pos < len) {
          memmove(buf + pos, buf + pos + 1, len - pos - 1);
          len--;
          buf[len] = '\0';
          refreshLine(prompt, buf, len, pos);
        }
        break;
        
      case VK_LEFT:
        if (pos > 0) {
          pos--;
          refreshLine(prompt, buf, len, pos);
        }
        break;
        
      case VK_RIGHT:
        if (pos < len) {
          pos++;
          refreshLine(prompt, buf, len, pos);
        }
        break;
        
      case VK_HOME:
        pos = 0;
        refreshLine(prompt, buf, len, pos);
        break;
        
      case VK_END:
        pos = len;
        refreshLine(prompt, buf, len, pos);
        break;
        
      case VK_UP:
        if (historyIndex > 0) {
          if (historyIndex == history.count && len > 0) {
            if (saved) free(saved);
            saved = strdup(buf);
          }
          historyIndex--;
          strncpy(buf, history.lines[historyIndex], WINLINE_MAX_LINE - 1);
          buf[WINLINE_MAX_LINE - 1] = '\0';
          len = (int)strlen(buf);
          pos = len;
          refreshLine(prompt, buf, len, pos);
        }
        break;
        
      case VK_DOWN:
        if (historyIndex < history.count) {
          historyIndex++;
          if (historyIndex == history.count) {
            if (saved) {
              strncpy(buf, saved, WINLINE_MAX_LINE - 1);
              buf[WINLINE_MAX_LINE - 1] = '\0';
              free(saved);
              saved = NULL;
            } else {
              buf[0] = '\0';
            }
          } else {
            strncpy(buf, history.lines[historyIndex], WINLINE_MAX_LINE - 1);
            buf[WINLINE_MAX_LINE - 1] = '\0';
          }
          len = (int)strlen(buf);
          pos = len;
          refreshLine(prompt, buf, len, pos);
        }
        break;
        
      case VK_ESCAPE:
        /* Clear line */
        len = 0;
        pos = 0;
        buf[0] = '\0';
        refreshLine(prompt, buf, len, pos);
        break;
        
      default:
        if (key->uChar.AsciiChar >= 32 && key->uChar.AsciiChar < 127) {
          if (len < WINLINE_MAX_LINE - 1) {
            if (pos < len) {
              memmove(buf + pos + 1, buf + pos, len - pos);
            }
            buf[pos] = key->uChar.AsciiChar;
            pos++;
            len++;
            buf[len] = '\0';
            refreshLine(prompt, buf, len, pos);
          }
        }
        break;
    }
  }
  
  if (saved) free(saved);
  return NULL;
}

int winlineHistoryAdd(const char *line) {
  char *copy;
  
  if (!line || line[0] == '\0') {
    return 0;
  }
  
  /* Don't add duplicate of last entry */
  if (history.count > 0 && strcmp(history.lines[history.count - 1], line) == 0) {
    return 0;
  }
  
  copy = strdup(line);
  if (!copy) {
    return -1;
  }
  
  if (history.lines == NULL) {
    history.lines = malloc(sizeof(char*) * history.max);
    if (!history.lines) {
      free(copy);
      return -1;
    }
  }
  
  if (history.count >= history.max) {
    /* Remove oldest entry */
    free(history.lines[0]);
    memmove(history.lines, history.lines + 1, sizeof(char*) * (history.max - 1));
    history.count = history.max - 1;
  }
  
  history.lines[history.count] = copy;
  history.count++;
  
  return 0;
}

int winlineHistorySetMaxLen(int len) {
  if (len < 1) {
    return -1;
  }
  
  if (len < history.count) {
    int toRemove = history.count - len;
    for (int i = 0; i < toRemove; i++) {
      free(history.lines[i]);
    }
    memmove(history.lines, history.lines + toRemove, sizeof(char*) * len);
    history.count = len;
  }
  
  history.max = len;
  return 0;
}

int winlineHistorySave(const char *filename) {
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    return -1;
  }
  
  for (int i = 0; i < history.count; i++) {
    fprintf(fp, "%s\n", history.lines[i]);
  }
  
  fclose(fp);
  return 0;
}

int winlineHistoryLoad(const char *filename) {
  FILE *fp = fopen(filename, "r");
  char buf[WINLINE_MAX_LINE];
  
  if (!fp) {
    return -1;
  }
  
  while (fgets(buf, sizeof(buf), fp)) {
    int len = (int)strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
    }
    winlineHistoryAdd(buf);
  }
  
  fclose(fp);
  return 0;
}

void winlineClearScreen(void) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  COORD coordScreen = {0, 0};
  DWORD charsWritten;
  DWORD consoleSize;
  
  if (hStdout == INVALID_HANDLE_VALUE) {
    return;
  }
  
  GetConsoleScreenBufferInfo(hStdout, &csbi);
  consoleSize = csbi.dwSize.X * csbi.dwSize.Y;
  
  FillConsoleOutputCharacter(hStdout, ' ', consoleSize, coordScreen, &charsWritten);
  GetConsoleScreenBufferInfo(hStdout, &csbi);
  FillConsoleOutputAttribute(hStdout, csbi.wAttributes, consoleSize, coordScreen, &charsWritten);
  SetConsoleCursorPosition(hStdout, coordScreen);
}
