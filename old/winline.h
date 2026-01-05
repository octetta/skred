// winline.h - Minimal line editing library for Windows
// A linenoise-like API using Windows Console API
// Pure C, no dependencies beyond standard library and Windows API

#ifndef WINLINE_H
#define WINLINE_H

int winlineInit(void);

void winlineCleanup(void);

// Read a line of input with editing and history support
// Returns a malloc'd string that must be freed by the caller
// Returns NULL on EOF or error
// Prompt is displayed before input
char *winlineReadLine(const char *prompt);

int winlineHistoryAdd(const char *line);
int winlineHistorySetMaxLen(int len);
int winlineHistorySave(const char *filename);
int winlineHistoryLoad(const char *filename);
void winlineClearScreen(void);

#endif
