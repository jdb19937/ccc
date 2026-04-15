/*
 * genera.h — declarationes generantis codicis
 */

#ifndef GENERA_H
#define GENERA_H

#include "ccc.h"

/* ================================================================
 * limites generantis
 * ================================================================ */

#define MAX_CASUS 512
#define MAX_BREAK 256

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
 * casus (pro switch)
 * ================================================================ */

typedef struct {
    long valor;
    int label;
} casus_t;

/* ================================================================
 * functiones
 * ================================================================ */

void genera_initia(void);
void genera_translatio(nodus_t *radix, const char *plica_exitus, int modus_objecti);

#endif /* GENERA_H */
