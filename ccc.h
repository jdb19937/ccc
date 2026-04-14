/*
 * ccc.h — CCC compilator caput
 *
 * Compilator C99 in C99 puro. Generat codicem ARM64,
 * scribit Mach-O directe. Nulla assemblatrix, nullus ligator.
 */

#ifndef CCC_H
#define CCC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ================================================================
 * limites communes
 * ================================================================ */

#define MAX_CHORDA 8192

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
    /* litterae et nomina */
    T_NUM, T_STR, T_CHARLIT, T_IDENT,
    T_NUM_SIGNORUM
};

/* ================================================================
 * signum (token)
 * ================================================================ */

typedef struct {
    int genus;
    long valor;             /* pro T_NUM, T_CHARLIT */
    char chorda[MAX_CHORDA]; /* pro T_STR, T_IDENT */
    int lon_chordae;        /* longitudo chordae pro T_STR */
    int linea;
} signum_t;

/* ================================================================
 * genera typorum
 * ================================================================ */

enum {
    TY_VOID, TY_CHAR, TY_SHORT, TY_INT, TY_LONG, TY_LLONG,
    TY_UCHAR, TY_USHORT, TY_UINT, TY_ULONG, TY_ULLONG,
    TY_FLOAT, TY_DOUBLE,
    TY_PTR, TY_ARRAY, TY_STRUCT, TY_ENUM, TY_FUNC,
};

/* ================================================================
 * typus
 * ================================================================ */

typedef struct typus typus_t;

typedef struct {
    char nomen[128];
    typus_t *typus;
    int offset;
} membrum_t;

struct typus {
    int genus;
    int magnitudo;          /* magnitudo in octetis */
    int colineatio;         /* colineatio in octetis */
    int est_sine_signo;     /* unsigned? */
    int est_constans;       /* const? */

    /* pro indicibus (ptr) et tabulis (array) */
    typus_t *basis;
    int num_elementorum;    /* pro tabulis; -1 si indefinitum */

    /* pro structuris */
    membrum_t *membra;
    int num_membrorum;
    char nomen_tag[128];
    int est_perfectum;      /* structura definita (non solum declarata) */

    /* pro functionibus */
    typus_t *reditus;
    typus_t **parametri;
    char **nomina_param;
    int num_parametrorum;
    int est_variadicus;
};

/* ================================================================
 * genera nodorum AST
 * ================================================================ */

enum {
    N_NUM, N_STR, N_CHARLIT, N_IDENT,
    N_BINOP, N_UNOP, N_POSTOP,
    N_ASSIGN, N_OPASSIGN,  /* = et +=, -=, etc. */
    N_TERNARY, N_COMMA_EXPR,
    N_CALL, N_INDEX, N_MEMBER, N_ARROW,
    N_CAST, N_SIZEOF_TYPE, N_SIZEOF_EXPR,
    N_ADDR, N_DEREF,
    N_BLOCK, N_IF, N_WHILE, N_DOWHILE, N_FOR,
    N_SWITCH, N_CASE, N_DEFAULT,
    N_RETURN, N_BREAK, N_CONTINUE,
    N_GOTO, N_LABEL,
    N_EXPR_STMT,
    N_VAR_DECL, N_FUNC_DEF,
    N_NOP,
};

/* ================================================================
 * nodus AST
 * ================================================================ */

typedef struct nodus nodus_t;
struct nodus {
    int genus;
    typus_t *typus;         /* typus expressionis */
    int linea;
    int op;                 /* operator (T_PLUS, etc.) */
    long valor;             /* valor numericus */
    char *nomen;            /* nomen identificatoris */
    char *chorda;           /* chorda litteralis */
    int lon_chordae;        /* longitudo chordae */

    nodus_t *sinister;      /* operandus sinister / conditio / objectum */
    nodus_t *dexter;        /* operandus dexter / corpus verum / index */
    nodus_t *tertius;       /* corpus falsum / incrementum */
    nodus_t *quartus;       /* corpus iterationis (for) */

    nodus_t **membra;       /* sententiae blocki / argumenta vocationis */
    int num_membrorum;

    typus_t *typus_decl;    /* typus declaratus (cast, sizeof, var_decl) */
    int est_staticus;
    int est_externus;

    /* indicem ad symbolum (salvatum a parsore dum ambitus vivit) */
    struct symbolum *sym;
};

/* ================================================================
 * genera symbolorum
 * ================================================================ */

enum {
    SYM_VAR, SYM_FUNC, SYM_TYPEDEF, SYM_ENUM_CONST,
    SYM_STRUCT_TAG, SYM_ENUM_TAG,
};

typedef struct symbolum symbolum_t;
struct symbolum {
    char nomen[256];
    typus_t *typus;
    int genus;              /* SYM_VAR, SYM_FUNC, etc. */
    int offset;             /* offset in acervo (locals) */
    int est_globalis;
    int est_staticus;
    int est_externus;
    int est_parametrus;
    int valor_enum;         /* pro SYM_ENUM_CONST */
    int got_index;          /* index in GOT pro functione externa */
    int globalis_index;     /* index in tabula globalium */
    symbolum_t *proximus;   /* catena in ambitu */
};

/* ================================================================
 * ambitus (scope)
 * ================================================================ */

typedef struct ambitus ambitus_t;
struct ambitus {
    symbolum_t *symbola;
    ambitus_t *parens;
    int proximus_offset;    /* proximus offset in acervo */
};

/* ================================================================
 * declarationes — ccc.c
 * ================================================================ */

void erratum(const char *fmt, ...);
void erratum_ad(int linea, const char *fmt, ...);
char *lege_plicam(const char *via, int *longitudo);
char *via_directoria(const char *via);

extern char *fons_directorium;

#endif /* CCC_H */
