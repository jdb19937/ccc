/*
 * goldbachiana.c — Coniectura Goldbachii
 *
 * Verificat quod omnis numerus par > 2 est summa duorum primorum.
 * Invenit decompositionem cum differentia minima.
 * Cruciatus compilatoris: qsort cum comparatoribus complexis,
 * bsearch cum conversione void*, functiones stdlib ut callbacks,
 * conversiones inter void* et typos concretos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define LIMES_PRAEDEFINITUM 1000
#define MAXIMA_MAGNITUDO    100001

/* ===== Tabula primorum globalis ===== */

static int tabula_primorum[MAXIMA_MAGNITUDO];
static int numerositas_primorum;
static bool est_primus_tab[MAXIMA_MAGNITUDO];

static void
computa_primos(int limes)
{
    memset(est_primus_tab, true, (size_t)(limes + 1));
    est_primus_tab[0] = false;
    if (limes > 0)
        est_primus_tab[1] = false;

    for (int i = 2; i * i <= limes; i++) {
        if (est_primus_tab[i]) {
            for (int k = i * i; k <= limes; k += i)
                est_primus_tab[k] = false;
        }
    }

    numerositas_primorum = 0;
    for (int i = 2; i <= limes; i++) {
        if (est_primus_tab[i])
            tabula_primorum[numerositas_primorum++] = i;
    }
}

/* ===== Comparatores pro qsort et bsearch ===== */

/*
 * Comparator simplex — conversio void* ad int*.
 * qsort et bsearch transmittunt const void*.
 */
static int
compara_integros(const void *sinistrum, const void *dextrum)
{
    int a = *(const int *)sinistrum;
    int b = *(const int *)dextrum;
    /*
     * Subtractio simplex (a - b) potest superfluere.
     * Comparatio per ternarium evitat hoc periculum.
     */
    return (a > b) - (a < b);
}

/*
 * Structura pro decompositione Goldbachii.
 */
typedef struct {
    int numerus;
    int primus_p;
    int primus_q;
    int differentia;
} DecompositioGoldbachii;

/*
 * Comparator structurarum: ordinat per differentiam,
 * deinde per numerum.
 * Demonstrat accessum membrorum per void* conversiones.
 */
static int
compara_decompositiones(const void *sinistrum, const void *dextrum)
{
    const DecompositioGoldbachii *a = (const DecompositioGoldbachii *)sinistrum;
    const DecompositioGoldbachii *b = (const DecompositioGoldbachii *)dextrum;

    /* Primum per differentiam */
    if (a->differentia != b->differentia)
        return (a->differentia > b->differentia) -
            (a->differentia < b->differentia);

    /* Deinde per numerum */
    return (a->numerus > b->numerus) - (a->numerus < b->numerus);
}

/* ===== Decompositio Goldbachii ===== */

/*
 * bsearch: quaerit primum in tabula ordinata.
 * Conversio eventus void* ad int*.
 */
static bool
est_in_tabula(int valor)
{
    void *inventum = bsearch(
        &valor,
        tabula_primorum,
        (size_t)numerositas_primorum,
        sizeof(tabula_primorum[0]),
        compara_integros
    );
    return inventum != NULL;
}

/*
 * Invenit decompositionem n = p + q cum minima differentia |p - q|.
 */
static DecompositioGoldbachii
decompone(int n)
{
    DecompositioGoldbachii optima = { n, 0, 0, n };

    for (int i = 0; i < numerositas_primorum; i++) {
        int p = tabula_primorum[i];
        if (p > n / 2)
            break;
        int q = n - p;
        if (est_in_tabula(q)) {
            int diff = q - p;
            if (diff < optima.differentia) {
                optima.primus_p    = p;
                optima.primus_q    = q;
                optima.differentia = diff;
            }
        }
    }

    return optima;
}

#define MAXIMI_RECORDI 200

int
main(int argc, char *argv[])
{
    int limes = LIMES_PRAEDEFINITUM;
    if (argc > 1) {
        char *finis;
        long valor = strtol(argv[1], &finis, 10);
        if (*finis != '\0' || valor < 4 || valor > 100000) {
            fprintf(stderr, "Error: argumentum invalidum\n");
            return 1;
        }
        limes = (int)valor;
    }

    computa_primos(limes);
    printf("Numeri primi usque ad %d: %d\n\n", limes, numerositas_primorum);

    /* Verificatio coniecturam Goldbachii */
    printf("Coniectura Goldbachii: omnis par > 2 = p + q (p, q primi)\n\n");

    int errores = 0;
    DecompositioGoldbachii *recordi = malloc(
        (size_t)MAXIMI_RECORDI * sizeof(DecompositioGoldbachii)
    );
    if (recordi == NULL) {
        fprintf(stderr, "Error: allocatio memoriae fallit\n");
        return 1;
    }
    int numerositas_recordorum = 0;

    for (int n = 4; n <= limes; n += 2) {
        DecompositioGoldbachii dec = decompone(n);

        if (dec.primus_p == 0) {
            printf("  FALLACIA: %d non potest decomponi!\n", n);
            errores++;
            continue;
        }

        /* Serva recordos cum maxima differentia */
        if (numerositas_recordorum < MAXIMI_RECORDI) {
            recordi[numerositas_recordorum++] = dec;
        }
    }

    /* Ordina recordos per differentiam descendentem per qsort */
    qsort(
        recordi, (size_t)numerositas_recordorum,
        sizeof(DecompositioGoldbachii),
        compara_decompositiones
    );

    /* Scribe decompositiones cum minima differentia (primi inter se proximi) */
    printf("Decompositiones cum minima differentia:\n");
    int scripta = 0;
    for (int i = 0; i < numerositas_recordorum && scripta < 15; i++) {
        if (recordi[i].differentia <= 2) {
            printf(
                "  %d = %d + %d (diff = %d)\n",
                recordi[i].numerus,
                recordi[i].primus_p,
                recordi[i].primus_q,
                recordi[i].differentia
            );
            scripta++;
        }
    }

    /* Scribe decompositiones cum maxima differentia */
    printf("\nDecompositiones cum maxima differentia:\n");
    for (
        int i = numerositas_recordorum - 1;
        i >= 0 && i > numerositas_recordorum - 11; i--
    ) {
        printf(
            "  %d = %d + %d (diff = %d)\n",
            recordi[i].numerus,
            recordi[i].primus_p,
            recordi[i].primus_q,
            recordi[i].differentia
        );
    }

    /* Distributio differentiarum */
    printf("\nDistributio differentiarum:\n");
    int census_0     = 0;
    int census_2     = 0;
    int census_maior = 0;
    for (int i = 0; i < numerositas_recordorum; i++) {
        if (recordi[i].differentia == 0)
            census_0++;
        else if (recordi[i].differentia <= 6)
            census_2++;
        else
            census_maior++;
    }
    printf("  diff = 0 (p = q):   %d\n", census_0);
    printf("  diff in [1,6]:      %d\n", census_2);
    printf("  diff > 6:           %d\n", census_maior);

    free(recordi);

    if (errores > 0) {
        fprintf(stderr, "\nError: %d numeri non decompositi\n", errores);
        return 1;
    }

    printf("\nConiectura Goldbachii verificata usque ad %d.\n", limes);
    return 0;
}
