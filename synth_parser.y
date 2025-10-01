%include { #include "tokens.h" }

%name SynthParser
%token_type {Token}
%extra_argument {void *ctx}
%syntax_error { fprintf(stderr,"Syntax error\n"); }

program ::= stmts.

stmts ::= stmts stmt.
stmts ::= stmt.

stmt ::= FUNC args_opt maybe_semi. {
    handle_function(ctx, yymsp[-2].minor.func, yymsp[-1].minor.args, yymsp[-1].minor.argcount);
}

stmt ::= BLOCK maybe_semi. {
    handle_block(ctx, yymsp[-1].minor.text);
}

maybe_semi ::= .
maybe_semi ::= COMMA.
maybe_semi ::= SEMI.

args_opt(A) ::= . {
    A.args = NULL; A.argcount = 0;
}
args_opt(A) ::= arglist(B). { A = B; }

arglist(A) ::= FLOAT(X). {
    A.args = malloc(sizeof(Token));
    A.args[0].fval = X.fval;
    A.argcount = 1;
}
arglist(A) ::= arglist(B) COMMA FLOAT(X). {
    B.args = realloc(B.args, (B.argcount+1)*sizeof(Token));
    B.args[B.argcount].fval = X.fval;
    B.argcount++;
    A = B;
}

%parse_accept { printf("ACCEPT\n"); }
