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

/* ambitus */
void         ambitus_intra(void);
void         ambitus_exi(void);
symbolum_t  *ambitus_quaere(const char *nomen, int genus);
symbolum_t  *ambitus_quaere_omnes(const char *nomen);
symbolum_t  *ambitus_adde(const char *nomen, int genus);
ambitus_t   *ambitus_currens(void);

#endif /* PARSER_H */
