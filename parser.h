/*
 * parser.h — declarationes parsoris
 */

#ifndef PARSER_H
#define PARSER_H

#include "ccc.h"

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

/* lexator */
void lex_initia(const char *nomen, const char *fons, int longitudo);
void lex_proximum(void);
int  lex_specta(void);
int  lex_est_typus(const char *nomen);
void lex_registra_typedef(const char *nomen);

extern signum_t sig;
extern int      sig_linea;

#endif /* PARSER_H */
