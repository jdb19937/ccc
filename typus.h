/*
 * typus.h — genera typōrum et structūra typī
 */

#ifndef TYPUS_H
#define TYPUS_H

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
 * functiones
 * ================================================================ */

#define MAX_TYPI 16384

typus_t *typus_novus(int genus);
typus_t *typus_indicem(typus_t *basis);
typus_t *typus_tabulam(typus_t *basis, int num);
int      typus_magnitudo(typus_t *t);
int      typus_colineatio(typus_t *t);
int      typus_est_integer(typus_t *t);
int      typus_est_index(typus_t *t);
int      typus_est_arithmeticus(typus_t *t);
typus_t *typus_basis_indicis(typus_t *t);

void typus_initia(void);

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

#endif /* TYPUS_H */
