/*
 * ccc.c — CCC compilator principale
 *
 * Functio main(), lectio plicarum, nuntii errorum.
 */

#include "ccc.h"

#include <errno.h>

/* ================================================================
 * status globalis
 * ================================================================ */

char *fons_directorium = NULL;

/* ================================================================
 * errores
 * ================================================================ */

void erratum(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ccc: erratum: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void erratum_ad(int linea, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ccc:%d: erratum: ", linea);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

/* ================================================================
 * lectio plicae
 * ================================================================ */

char *lege_plicam(const char *via, int *longitudo)
{
    FILE *fp = fopen(via, "rb");
    if (!fp) {
        fprintf(
            stderr, "ccc: non possum aperire '%s': %s\n",
            via, strerror(errno)
        );
        exit(1);
    }

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

/* ================================================================
 * usus
 * ================================================================ */

static void usus(void)
{
    fprintf(stderr, "usus: ccc [-o exitus] fons.c\n");
    exit(1);
}

/* ================================================================
 * principale
 * ================================================================ */

int main(int argc, char *argv[])
{
    const char *plica_fontis = NULL;
    const char *plica_exitus = "a.out";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc)
                usus();
            plica_exitus = argv[i];
        } else if (argv[i][0] == '-') {
            /* ignora vexilla ignota */
        } else {
            plica_fontis = argv[i];
        }
    }

    if (!plica_fontis)
        usus();

    /* lege fontem */
    int longitudo;
    char *fons       = lege_plicam(plica_fontis, &longitudo);
    fons_directorium = via_directoria(plica_fontis);

    /* initia lexatorem */
    lex_initia(plica_fontis, fons, longitudo);

    /* initia parserem et parse */
    parse_initia();
    nodus_t *radix = parse_translatio();

    /* genera codicem */
    genera_initia();
    genera_translatio(radix, plica_exitus);

    free(fons);
    free(fons_directorium);
    return 0;
}
