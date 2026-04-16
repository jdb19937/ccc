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

void erratum(const char *fmt, ...);
void erratum_ad(int linea, const char *fmt, ...);
char *lege_plicam(const char *via, int *longitudo);
char *via_directoria(const char *via);

#endif /* UTILIA_H */
