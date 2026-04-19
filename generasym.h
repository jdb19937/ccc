/*
 * generasym.h — generator symbolicus (emittit assembly textum)
 */

#ifndef GENERASYM_H
#define GENERASYM_H

#include <stdio.h>
#include "parser.h"

/* limites eadem ac genera.c */
#define MAX_CASUS 1024
#define MAX_BREAK 256

/* casus (pro switch) — duplicātum ex genera.h ad independentem compilātiōnem */
typedef struct {
    long valor;
    int label;
} casus_t;

/* conditiones ARM64 (duplicantur ex genera.h ad independentem
 * compilationem) */
enum {
    GSYM_COND_EQ = 0, GSYM_COND_NE = 1,
    GSYM_COND_HS = 2, GSYM_COND_LO = 3,
    GSYM_COND_MI = 4, GSYM_COND_PL = 5,
    GSYM_COND_VS = 6, GSYM_COND_VC = 7,
    GSYM_COND_HI = 8, GSYM_COND_LS = 9,
    GSYM_COND_GE = 10, GSYM_COND_LT = 11,
    GSYM_COND_GT = 12, GSYM_COND_LE = 13,
    GSYM_COND_AL = 14,
};

void generasym_initia(void);
void generasym_translatio(nodus_t *radix, FILE *out);

#endif /* GENERASYM_H */
