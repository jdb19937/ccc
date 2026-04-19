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
    int campus_bitorum;   /* §6.7.2.1: 0 = nōn est campus; >0 = latitūdō in bitīs */
    int campus_positus;   /* positus in ūnitāte repositōriā (in bitīs) */
    int campus_signatus;  /* 1 sī signed — prō extensiōne signī */
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
    struct nodus *vla_dim;  /* §6.7.5.2: expressio dimensiōnis VLA (0 sī nōn) */

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

typus_t   *typus_novus(int genus);
typus_t   *typus_indicem(typus_t *basis);
typus_t   *typus_tabulam(typus_t *basis, int num);
int        typus_magnitudo(const typus_t *t);
int        typus_colineatio(const typus_t *t);
int        typus_est_integer(const typus_t *t);
int        typus_est_fluat(const typus_t *t);
int        typus_est_index(const typus_t *t);
int        typus_est_arithmeticus(const typus_t *t);
typus_t   *typus_basis_indicis(typus_t *t);
int        est_unsigned(const typus_t *t);
int        mag_typi(const typus_t *t);
int        mag_typi_verus(const typus_t *t);
int        typus_habet_vla(const typus_t *t);
int        numera_elementa_init(const typus_t *t);
membrum_t *quaere_membrum(typus_t *st, const char *nomen);


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
