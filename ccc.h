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
 * limites
 * ================================================================ */

#define MAX_SIGNA       262144
#define MAX_CHORDA      8192
#define MAX_NODI        262144
#define MAX_TYPI        16384
#define MAX_SYMBOLA     8192
#define MAX_MEMBRA      512
#define MAX_PARAM       64
#define MAX_CODEX       (4*1048576)
#define MAX_DATA        (2*1048576)
#define MAX_CHORDAE_LIT 16384
#define MAX_GOT         512
#define MAX_FIXUPS      262144
#define MAX_LABELS      65536
#define MAX_MACRAE      2048
#define MAX_PLICAE_ACERVUS 64
#define MAX_GLOBALES    4096
#define MAX_CASUS       512
#define MAX_AMBITUS     1024
#define MAX_BREAK       256

#define PAGINA          0x4000    /* ARM64 macOS pagina = 16 KiB */
#define VM_BASIS        0x100000000ULL

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
 * fixup (relocationes)
 * ================================================================ */

enum {
    FIX_ADRP,              /* ADRP ad chorda/datum */
    FIX_ADD_LO12,          /* ADD #lo12 ad chorda/datum */
    FIX_ADRP_GOT,          /* ADRP ad intransum GOT */
    FIX_LDR_GOT_LO12,     /* LDR [x, #lo12] ad intransum GOT */
    FIX_BRANCH,            /* B ad label */
    FIX_BCOND,             /* B.cond ad label */
    FIX_BL,                /* BL ad label */
    FIX_CBZ,               /* CBZ ad label */
    FIX_CBNZ,              /* CBNZ ad label */
    FIX_ADRP_DATA,         /* ADRP ad datum globale */
    FIX_ADD_LO12_DATA,     /* ADD #lo12 ad datum globale */
    FIX_LDR_LO12_DATA,    /* LDR [x, #lo12] ad datum globale */
    FIX_STR_LO12_DATA,    /* STR [x, #lo12] ad datum globale */
    FIX_ADR_LABEL,         /* ADR Xn, label (adresse codicis) */
};

typedef struct {
    int genus;
    int offset;             /* offset in codice */
    int target;             /* label / chorda_id / got_id / glob_id */
    int magnitudo_accessus; /* pro LDR/STR: 1,2,4,8 */
} fixup_t;

/* ================================================================
 * intransus GOT
 * ================================================================ */

typedef struct {
    char nomen[256];        /* nomen symboli (cum _ praefixo) */
} got_intrans_t;

/* ================================================================
 * chorda litteralis
 * ================================================================ */

typedef struct {
    char *data;
    int longitudo;
    int offset;             /* offset in sectione __cstring */
} chorda_lit_t;

/* ================================================================
 * variabilis globalis
 * ================================================================ */

typedef struct {
    char nomen[256];
    typus_t *typus;
    int magnitudo;
    int colineatio;
    int est_bss;
    int bss_offset;
    int data_offset;
    int est_staticus;
    long valor_initialis;   /* pro simplicibus initialibus */
    int habet_valorem;
} globalis_t;

/* ================================================================
 * casus (pro switch)
 * ================================================================ */

typedef struct {
    long valor;
    int label;
} casus_t;

/* ================================================================
 * conditiones ARM64
 * ================================================================ */

enum {
    COND_EQ = 0, COND_NE = 1,
    COND_HS = 2, COND_LO = 3,
    COND_MI = 4, COND_PL = 5,
    COND_VS = 6, COND_VC = 7,
    COND_HI = 8, COND_LS = 9,
    COND_GE = 10, COND_LT = 11,
    COND_GT = 12, COND_LE = 13,
    COND_AL = 14,
};

/* ================================================================
 * registra ARM64
 * ================================================================ */

#define SP  31
#define XZR 31
#define FP  29
#define LR  30

/* ================================================================
 * constantiae Mach-O
 * ================================================================ */

#define MH_MAGIC_64         0xFEEDFACF
#define MH_EXECUTE          2
#define CPU_TYPE_ARM64      (0x01000000 | 12)
#define CPU_SUBTYPE_ALL     0

#define LC_SEGMENT_64       0x19
#define LC_SYMTAB           0x02
#define LC_DYSYMTAB         0x0B
#define LC_LOAD_DYLINKER    0x0E
#define LC_LOAD_DYLIB       0x0C
#define LC_MAIN             (0x80000000 | 0x28)
#define LC_DYLD_INFO_ONLY   (0x80000000 | 0x22)
#define LC_BUILD_VERSION    0x32
#define LC_UUID             0x1B

#define MH_PIE              0x200000
#define MH_DYLDLINK         0x4
#define MH_TWOLEVEL         0x80
#define MH_NOUNDEFS         0x1

#define VM_PROT_NONE        0
#define VM_PROT_READ        1
#define VM_PROT_WRITE       2
#define VM_PROT_EXECUTE     4

#define S_REGULAR           0x0
#define S_CSTRING_LITERALS  0x2
#define S_ZEROFILL          0x1
#define S_NON_LAZY_SYMBOL_POINTERS 0x6
#define S_ATTR_PURE_INSTRUCTIONS   0x80000000
#define S_ATTR_SOME_INSTRUCTIONS   0x00000400

#define BIND_OPCODE_DONE                          0x00
#define BIND_OPCODE_SET_DYLIB_ORDINAL_IMM         0x10
#define BIND_OPCODE_SET_TYPE_IMM                  0x50
#define BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB   0x70
#define BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM 0x40
#define BIND_OPCODE_DO_BIND                       0x90

#define BIND_TYPE_POINTER   1

#define REBASE_OPCODE_DONE  0x00

#define N_EXT               0x01
#define N_UNDF              0x0
#define N_SECT              0xE

#define EXPORT_SYMBOL_FLAGS_KIND_REGULAR 0x00

#define PLATFORM_MACOS      1
#define TOOL_LD             3

/* ================================================================
 * declarationes — ccc.c
 * ================================================================ */

void erratum(const char *fmt, ...);
void erratum_ad(int linea, const char *fmt, ...);
char *lege_plicam(const char *via, int *longitudo);
char *via_directoria(const char *via);

extern char *fons_directorium;

/* ================================================================
 * declarationes — lexator.c
 * ================================================================ */

void lex_initia(const char *nomen, const char *fons, int longitudo);
void lex_proximum(void);
int  lex_specta(void);
int  lex_est_typus(const char *nomen);
void lex_registra_typedef(const char *nomen);

extern signum_t sig;           /* signum currens */
extern int      sig_linea;     /* linea currens */

/* ================================================================
 * declarationes — parser.c
 * ================================================================ */

void     parse_initia(void);
nodus_t *parse_translatio(void);
nodus_t *nodus_novus(int genus);
typus_t *typus_novus(int genus);
typus_t *typus_indicem(typus_t *basis);
typus_t *typus_tabulam(typus_t *basis, int num);
int      typus_magnitudo(typus_t *t);
int      typus_colineatio(typus_t *t);
int      typus_est_integer(typus_t *t);
int      typus_est_index(typus_t *t);
int      typus_est_arithmeticus(typus_t *t);
typus_t *typus_basis_indicis(typus_t *t);

/* typi praefiniti */
extern typus_t *ty_void;
extern typus_t *ty_char;
extern typus_t *ty_uchar;
extern typus_t *ty_short;
extern typus_t *ty_ushort;
extern typus_t *ty_int;
extern typus_t *ty_uint;
extern typus_t *ty_long;
extern typus_t *ty_ulong;

/* ambitus */
void         ambitus_intra(void);
void         ambitus_exi(void);
symbolum_t  *ambitus_quaere(const char *nomen, int genus);
symbolum_t  *ambitus_quaere_omnes(const char *nomen);
symbolum_t  *ambitus_adde(const char *nomen, int genus);
ambitus_t   *ambitus_currens(void);

/* ================================================================
 * declarationes — genera.c
 * ================================================================ */

void genera_initia(void);
void genera_translatio(nodus_t *radix, const char *plica_exitus);

#endif /* CCC_H */
