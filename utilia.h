/*
 * utilia.h — functiōnēs commūnēs
 */

#ifndef UTILIA_H
#define UTILIA_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char *nomen_programmi;
extern const char *plica_currentis;  /* plica fontis currens; NULL si ignota */
extern const char *plica_exitus_gl;  /* plica exitus delenda si erratum accidit */
extern const char *plica_i_tmp_gl;   /* plica .i temporaria delenda si erratum */

void erratum(const char *fmt, ...);
void erratum_ad(int linea, const char *fmt, ...);
char *lege_plicam(const char *via, int *longitudo);

/* redde 1 si chorda n octetis est UTF-8 valida, 0 aliter */
int utf8_valida(const char *s, int n);

/* macro prō crescentiā seriērum dynāmicārum (realloc cum duplicātiōne) */
#define CRESC_SERIEM(arr, numerus, capac, typus)            \
    do {                                                    \
        if ((numerus) >= (capac)) {                         \
            (capac) = (capac) ? (capac) * 2 : 8;           \
            (arr) = realloc((arr), (capac) * sizeof(typus));\
            if (!(arr))                                     \
                erratum("memoria exhausta");                \
        }                                                   \
    } while (0)

#endif /* UTILIA_H */
