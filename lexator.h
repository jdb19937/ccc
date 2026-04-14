/*
 * lexator.h — declarationes lexatoris et praeprocessoris
 */

#ifndef LEXATOR_H
#define LEXATOR_H

#include "ccc.h"

/* ================================================================
 * limites lexatoris
 * ================================================================ */

#define MAX_SIGNA          262144
#define MAX_MACRAE         2048
#define MAX_PLICAE_ACERVUS 64

/* ================================================================
 * functiones
 * ================================================================ */

void lex_lege_capita(const char *via_exec);
void lex_initia(const char *nomen, const char *fons, int longitudo);
void lex_proximum(void);
int  lex_specta(void);
int  lex_est_typus(const char *nomen);
void lex_registra_typedef(const char *nomen);

extern signum_t sig;
extern int      sig_linea;

#endif /* LEXATOR_H */
