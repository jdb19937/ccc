/*
 * typus.h — utilia typorum
 */

#ifndef TYPUS_H
#define TYPUS_H

#include "ccc.h"

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
