/*
 * cribrum.c — Cribrum Eratosthenis
 *
 * Numeros primos invenit per cribrum antiquum.
 * Cruciatus compilatoris: arietes longitudinis variabilis (VLA),
 * qualificator 'static' in parametro arietis, sizeof(VLA)
 * tempore executionis computatus, plures VLA in eodem ambitu.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define LIMES_PRAEDEFINITUS 1000

/*
 * Duplicitas vocis 'static': primo pro nexu interno functionis,
 * secundo intra brackets parametri — compilatori significat
 * minimum unum elementum praesens esse (C99 6.7.5.3).
 */
static void
cerne_cribrum(int magnitudo, bool cribrum[static 1])
{
    if (magnitudo < 2)
        return;
    cribrum[0] = false;
    cribrum[1] = false;

    for (int i = 2; i * i < magnitudo; i++) {
        if (cribrum[i]) {
            for (int k = i * i; k < magnitudo; k += i)
                cribrum[k] = false;
        }
    }
}

/* VLA per indicatorem et longitudinem separatam */
static int
numera_primos(int magnitudo, const bool *cribrum)
{
    int summa = 0;
    for (int i = 2; i < magnitudo; i++) {
        if (cribrum[i])
            summa++;
    }
    return summa;
}

/*
 * Plures VLA in eodem ambitu functionis.
 * Uterque aries tempore executionis in acervo allocatur.
 */
static void
inveni_gemellos(int limes)
{
    int magnitudo = limes + 1;
    bool est_primus[magnitudo];       /* VLA primus */
    int gemelli_sinistri[magnitudo];  /* VLA secundus */

    /* sizeof VLA — valor tempore executionis computatur */
    printf(
        "Magnitudo VLA booliani: %zu octeti (%d elementa)\n",
        sizeof(est_primus), magnitudo
    );

    memset(est_primus, 1, sizeof(est_primus));
    cerne_cribrum(magnitudo, est_primus);

    int numerositas = 0;
    for (int i = 2; i + 2 < magnitudo; i++) {
        if (est_primus[i] && est_primus[i + 2])
            gemelli_sinistri[numerositas++] = i;
    }

    printf("Primi gemelli usque ad %d:\n", limes);
    for (int i = 0; i < numerositas; i++) {
        int p = gemelli_sinistri[i];
        printf("  (%d, %d)", p, p + 2);
        if ((i + 1) % 6 == 0)
            putchar('\n');
    }
    if (numerositas % 6 != 0)
        putchar('\n');
    printf("Numerositas parium gemellorum: %d\n", numerositas);
}

int
main(int argc, char *argv[])
{
    int limes = LIMES_PRAEDEFINITUS;

    if (argc > 1) {
        char *finis;
        long valor = strtol(argv[1], &finis, 10);
        if (*finis != '\0' || valor < 2 || valor > 50000) {
            fprintf(stderr, "Error: argumentum invalidum '%s'\n", argv[1]);
            return 1;
        }
        limes = (int)valor;
    }

    int magnitudo = limes + 1;

    /* VLA principalis in main() */
    bool cribrum[magnitudo];
    printf("Magnitudo cribri: %zu octeti\n", sizeof(cribrum));

    memset(cribrum, 1, sizeof(cribrum));
    cerne_cribrum(magnitudo, cribrum);

    printf("\nNumeri primi usque ad %d:\n", limes);
    int per_lineam = 0;
    for (int i = 2; i < magnitudo; i++) {
        if (cribrum[i]) {
            printf("%6d", i);
            if (++per_lineam % 12 == 0)
                putchar('\n');
        }
    }
    if (per_lineam % 12 != 0)
        putchar('\n');

    printf("Numerositas primorum: %d\n\n", numera_primos(magnitudo, cribrum));

    /* VLA in ambitu interiore — cribrum separatum */
    {
        int parva_magnitudo = (limes < 200) ? limes + 1 : 201;
        bool cribrum_parvum[parva_magnitudo];  /* VLA in ambitu interiore */
        memset(cribrum_parvum, 1, sizeof(cribrum_parvum));
        cerne_cribrum(parva_magnitudo, cribrum_parvum);

        printf("Quadrata primorum usque ad %d:\n", parva_magnitudo - 1);
        int scriptum = 0;
        for (int i = 2; i < parva_magnitudo; i++) {
            if (cribrum_parvum[i]) {
                printf("  %d^2 = %d", i, i * i);
                if (++scriptum % 5 == 0)
                    putchar('\n');
            }
        }
        if (scriptum % 5 != 0)
            putchar('\n');
    }

    putchar('\n');
    inveni_gemellos(limes);

    return 0;
}
