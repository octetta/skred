#include "synth_parser.h"  // Must come first for TOKEN_* macros
#include "tokens.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ---- Handlers ----
void handle_function(void *ctx, char func, Token *args, int argcount) {
    printf("Function %c(", func);
    for(int i=0; i<argcount; i++)
        printf("%s%g", i?", ":"", args[i].fval);
    printf(")\n");
    if(args) free(args);
}

void handle_block(void *ctx, char *text) {
    printf("Block {%s}\n", text);
    free(text);
}

// ---- Lexer state ----
typedef struct {
    char buf[2048];
    size_t len;
    size_t pos;
} LexerState;

void lexer_init(LexerState *st) { st->len=st->pos=0; st->buf[0]='\0'; }

void lexer_feed(LexerState *st, const char *chunk, size_t n) {
    if(st->len + n >= sizeof(st->buf)) { fprintf(stderr,"Lexer buffer overflow\n"); exit(1);}
    memcpy(st->buf + st->len, chunk, n);
    st->len += n;
    st->buf[st->len] = '\0';
}

// ---- Lexer ----
int lexer_next(LexerState *st, Token *tok) {
    while(st->pos < st->len){
        char c = st->buf[st->pos];

        // skip whitespace
        if(isspace((unsigned char)c)){ st->pos++; continue; }

        // skip comments
        if(c=='#'){ while(st->pos<st->len && st->buf[st->pos]!='\n') st->pos++; continue; }

        // function name
        if(isalpha((unsigned char)c)){ tok->func = c; st->pos++; return TOKEN_FUNC; }

        // float
        if(isdigit((unsigned char)c)||c=='-'||c=='.'){
            char *endptr;
            tok->fval = strtod(&st->buf[st->pos], &endptr);
            if(endptr==&st->buf[st->pos]) return TOKEN_END;
            st->pos = endptr - st->buf;
            return TOKEN_FLOAT;
        }

        // comma, semicolon
        if(c==','){ st->pos++; return TOKEN_COMMA; }
        if(c==';'){ st->pos++; return TOKEN_SEMI; }

        // block
        if(c=='{'){
            st->pos++;
            size_t start = st->pos;
            while(st->pos < st->len && st->buf[st->pos] != '}') st->pos++;
            if(st->pos >= st->len){ st->pos = start-1; return TOKEN_END; } // incomplete
            size_t len = st->pos - start;
            tok->text = malloc(len+1);
            memcpy(tok->text, &st->buf[start], len);
            tok->text[len] = '\0';
            st->pos++; // skip closing brace
            return TOKEN_BLOCK;
        }

        fprintf(stderr,"Unknown char: %c\n",c); st->pos++;
    }
    return TOKEN_END;
}

void lexer_compact(LexerState *st){
    if(st->pos>0){ memmove(st->buf, st->buf+st->pos, st->len-st->pos); st->len-=st->pos; st->pos=0; st->buf[st->len]='\0'; }
}

// ---- Parser driver ----
void parse_stream(void *parser, LexerState *st){
    Token tok;
    int token;
    while((token = lexer_next(st,&tok))>0)
        SynthParser(parser, token, tok, NULL);
    lexer_compact(st);
}

// ---- Demo main ----
int main(void){
    void *parser = SynthParserAlloc(malloc);
    LexerState st;
    lexer_init(&st);

    const char *chunks[] = {
        "f1,2 w3,4 # comment\n",
        "a {code goes here}",
        NULL
    };

    for(int i=0; chunks[i]; i++){
        printf("Feeding chunk: \"%s\"\n", chunks[i]);
        lexer_feed(&st, chunks[i], strlen(chunks[i]));
        parse_stream(parser, &st);
    }

    Token tok;
    SynthParser(parser, TOKEN_END, tok, NULL);
    SynthParserFree(parser, free);

    return 0;
}
