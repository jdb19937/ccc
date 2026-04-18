/*
 * utilia.c — functiōnēs commūnēs
 */

#include "utilia.h"

#include <errno.h>

const char *nomen_programmi = "ccc";
const char *plica_currentis = NULL;
const char *plica_exitus_gl = NULL;

void erratum(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s: erratum: ", nomen_programmi);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    if (plica_exitus_gl)
        remove(plica_exitus_gl);
    exit(1);
}

void erratum_ad(int linea, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const char *loc = plica_currentis ? plica_currentis : nomen_programmi;
    fprintf(stderr, "%s:%d: erratum: ", loc, linea);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    if (plica_exitus_gl)
        remove(plica_exitus_gl);
    exit(1);
}

char *lege_plicam(const char *via, int *longitudo)
{
    FILE *fp = fopen(via, "rb");
    if (!fp)
        erratum("non possum aperire '%s': %s", via, strerror(errno));

    fseek(fp, 0, SEEK_END);
    long mag = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = malloc(mag + 1);
    if (!data)
        erratum("memoria exhausta");
    fread(data, 1, mag, fp);
    data[mag] = '\0';
    fclose(fp);

    if (longitudo)
        *longitudo = (int)mag;
    return data;
}

char *via_directoria(const char *via)
{
    char *copia = strdup(via);
    char *ult   = strrchr(copia, '/');
    if (ult) {
        *(ult + 1) = '\0';
    } else {
        free(copia);
        copia = strdup("./");
    }
    return copia;
}
