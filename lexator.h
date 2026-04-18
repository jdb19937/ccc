/*
 * lexator.h — declarationes lexatoris
 */

#ifndef LEXATOR_H
#define LEXATOR_H

/* ================================================================
 * limites lexatoris
 * ================================================================ */

#define MAX_CHORDA         4096   /* §5.2.4.1: minimum 4095 characters in string literal */
#define MAX_SIGNA          262144

/* ================================================================
 * genera signorum (token types)
 * ================================================================ */

enum {
    T_EOF = 0,
    /* vocabula clavis */
    T_AUTO, T_BREAK, T_CASE, T_CHAR, T_CONST, T_CONTINUE, T_DEFAULT,
    T_DO, T_DOUBLE, T_ELSE, T_ENUM, T_EXTERN, T_FLOAT, T_FOR, T_GOTO,
    T_IF, T_INT, T_LONG, T_REGISTER, T_RETURN, T_SHORT, T_SIGNED,
    T_SIZEOF, T_STATIC, T_STRUCT, T_SWITCH, T_TYPEDEF, T_UNION,
    T_UNSIGNED, T_VOID, T_VOLATILE, T_WHILE,
    /* operatores et punctuatio */
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_AMP, T_PIPE, T_CARET, T_TILDE, T_BANG,
    T_LT, T_GT, T_ASSIGN,
    T_PLUSPLUS, T_MINUSMINUS,
    T_LTEQ, T_GTEQ, T_EQEQ, T_BANGEQ,
    T_AMPAMP, T_PIPEPIPE,
    T_LTLT, T_GTGT,
    T_PLUSEQ, T_MINUSEQ, T_STAREQ, T_SLASHEQ, T_PERCENTEQ,
    T_AMPEQ, T_PIPEEQ, T_CARETEQ, T_LTLTEQ, T_GTGTEQ,
    T_ARROW, T_DOT, T_QUESTION, T_COLON, T_SEMICOLON, T_COMMA,
    T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET, T_LBRACE, T_RBRACE,
    T_ELLIPSIS, T_HASH,
    /* vocabula clavis C99 addita — §6.7.4, §6.2.5, §6.7.3.1 */
    T_INLINE, T_BOOL, T_RESTRICT,
    /* litterae et nomina */
    T_NUM, T_NUM_FLUAT, T_STR, T_CHARLIT, T_IDENT,
    T_NUM_SIGNORUM
};

/* ================================================================
 * signum (token)
 * ================================================================ */

typedef struct {
    int genus;
    long valor;             /* pro T_NUM, T_CHARLIT */
    double valor_f;         /* pro T_NUM_FLUAT — §6.4.4.2 */
    char chorda[MAX_CHORDA]; /* pro T_STR, T_IDENT */
    int lon_chordae;        /* longitudo chordae pro T_STR */
    int linea;
} signum_t;

/* ================================================================
 * functiones
 * ================================================================ */

void lex_initia(const char *nomen, const char *fons, int longitudo);
void lex_proximum(void);
int  lex_specta(void);
int  lex_est_typus(const char *nomen);
void lex_registra_typedef(const char *nomen);

extern signum_t sig;
extern int      sig_linea;

#endif /* LEXATOR_H */
