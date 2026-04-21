/*
 * gsymconst.h — evaluatio expressionum constantium pro initializatoribus
 * globalibus (§6.6). Extractum ex generasym.c.
 */
#ifndef GSYMCONST_H
#define GSYMCONST_H

#include "parser.h"

int    gsymconst_continet_fluat(nodus_t *n);
double gsymconst_evalua_fluat(nodus_t *n);
long   gsymconst_evalua_integer(nodus_t *n);

#endif
