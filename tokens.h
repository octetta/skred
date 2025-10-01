#ifndef TOKENS_H
#define TOKENS_H

typedef struct {
    char func;       // for FUNC
    double fval;     // for FLOAT
    char *text;      // for BLOCK
} Token;

#endif
