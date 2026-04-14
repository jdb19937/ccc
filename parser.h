/*
 * parser.h — declarationes parsoris
 */

#ifndef PARSER_H
#define PARSER_H

#include "ccc.h"
#include "lexator.h"

/* ================================================================
 * limites parsoris
 * ================================================================ */

#define MAX_NODI    262144
#define MAX_TYPI    16384
#define MAX_SYMBOLA 8192
#define MAX_MEMBRA  512
#define MAX_PARAM   64
#define MAX_AMBITUS 1024

/* ================================================================
 * functiones
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

#endif /* PARSER_H */
