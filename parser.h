/*
 * parser.h — declarationes parsoris
 */

#ifndef PARSER_H
#define PARSER_H

#include "typus.h"
#include "lexator.h"

/* ================================================================
 * limites parsoris
 * ================================================================ */

#define MAX_NODI    262144
#define MAX_SYMBOLA 8192
#define MAX_MEMBRA  1024  /* §5.2.4.1: minimum 1023 members in struct/union */
#define MAX_PARAM   128   /* §5.2.4.1: minimum 127 parameters */
#define MAX_AMBITUS 1024  /* §5.2.4.1: minimum 127 nesting levels of blocks */

/* ================================================================
 * genera nodorum AST
 * ================================================================ */

enum {
    N_NUM, N_NUM_FLUAT, N_STR, N_CHARLIT, N_IDENT,
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
    N_VA_START, N_VA_ARG, N_VA_END,
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
    double valor_f;         /* valor fluitans — §6.4.4.2 */
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
 * functiones
 * ================================================================ */

void     parse_initia(void);
nodus_t *parse_translatio(void);
nodus_t *nodus_novus(int genus);
/* ambitus */
void         ambitus_intra(void);
void         ambitus_exi(void);
symbolum_t  *ambitus_quaere(const char *nomen, int genus);
symbolum_t  *ambitus_quaere_omnes(const char *nomen);
symbolum_t  *ambitus_adde(const char *nomen, int genus);
ambitus_t   *ambitus_currens(void);

#endif /* PARSER_H */
