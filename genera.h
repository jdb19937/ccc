/*
 * genera.h — declarationes generantis codicis
 */

#ifndef GENERA_H
#define GENERA_H

#include "ccc.h"

typedef struct {
    char nomen[256];
    int label;
} func_loc_t;

extern func_loc_t func_loci[];
extern int num_func_loc;

void genera_initia(void);
void genera_translatio(nodus_t *radix, const char *plica_exitus, int modus_objecti);
int  func_loc_quaere(const char *nomen);
int  func_loc_adde(const char *nomen);

#endif /* GENERA_H */
