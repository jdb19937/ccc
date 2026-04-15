/*
 * func.h — tabula functionum localium
 */

#ifndef FUNC_H
#define FUNC_H

#include "emitte.h"

typedef struct {
    char nomen[256];
    int label;
    int est_staticus;
} func_loc_t;

extern func_loc_t func_loci[];
extern int num_func_loc;

int  func_loc_quaere(const char *nomen);
int  func_loc_adde(const char *nomen, int est_staticus);

#endif /* FUNC_H */
