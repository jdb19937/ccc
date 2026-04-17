/*
 * coniectura.c — Coniectura Collatzii (3n+1)
 *
 * Explorat sequentias Collatzii et recordos longitudinis invenit.
 * Cruciatus compilatoris: 'long long' et 'unsigned long long',
 * operator commatis in expressionibus complexis, catenae ternarii
 * operatoris, promotiones typorum inter signatos et non-signatos,
 * format specifiers %lld et %llu.
 */

#include <stdio.h>
#include <stdlib.h>

/* ===== Typi ===== */

typedef unsigned long long Naturalis;
typedef long long Integer;

/* ===== Operationes cum ternario et commate ===== */

/*
 * Passus Collatzii per ternarium nestificatum:
 * si par -> n/2, si impar -> 3n+1
 */
static inline Naturalis
passus_collatzii(Naturalis n)
{
    return (n % 2 == 0) ? n / 2 : 3 * n + 1;
}

/*
 * Longitudo sequentiae Collatzii usque ad 1.
 * Operator commatis in incremento plicae 'for':
 * ambae variabiles simul mutantur.
 */
static Integer
longitudo_sequentiae(Naturalis initium)
{
    if (initium <= 1)
        return 0;

    Naturalis valor = initium;
    Integer passus;

    /*
     * Operator commatis: 'valor = passus_collatzii(valor), passus++'
     * — uterque effectus lateralis in una expressione.
     */
    for (passus = 0; valor != 1; valor = passus_collatzii(valor), passus++)
        (void)0;  /* corpus vacuum — omne opus fit in capite plicae */

    return passus;
}

/*
 * Computat altitudinem maximam (maximum valor in sequentia).
 * Catenae ternarii pro classificatione.
 */
static Naturalis
altitudo_maxima(Naturalis initium)
{
    Naturalis valor   = initium;
    Naturalis maximum = initium;

    while (valor != 1) {
        valor   = passus_collatzii(valor);
        maximum = (valor > maximum) ? valor : maximum;
    }

    return maximum;
}

/*
 * Classificatio numeri per catenam ternarii.
 */
static const char *
    classifica_numerum(Naturalis n)
{
    return (
        (n == 0)     ? "nullus" :
        (n == 1)     ? "unitas" :
        (n % 2 == 0) ? "par" :
        (n % 3 == 0) ? "impar triplicis" :
        (n % 5 == 0) ? "impar quintuplicis" :
        "impar alius"
    );
}

/*
 * Scribe sequentiam completam — demonstrat promotiones typorum.
 */
static void
scribe_sequentiam(Naturalis initium)
{
    printf("  Sequentia(%llu):", initium);
    Naturalis valor = initium;
    int scriptum    = 0;

    while (valor != 1) {
        printf(" %llu", valor);
        if (++scriptum % 15 == 0)
            printf("\n    ");
        valor = passus_collatzii(valor);
    }
    printf(" 1\n");
}

#define LIMES_PRAEDEFINITUM 1000

int
main(int argc, char *argv[])
{
    Naturalis limes = LIMES_PRAEDEFINITUM;
    if (argc > 1) {
        char *finis;
        unsigned long long valor = strtoull(argv[1], &finis, 10);
        if (*finis != '\0' || valor < 2 || valor > 10000000ULL) {
            fprintf(stderr, "Error: argumentum invalidum\n");
            return 1;
        }
        limes = (Naturalis)valor;
    }

    printf("Coniectura Collatzii usque ad %llu:\n\n", limes);

    /* Inveni recordos longitudinis */
    Integer longitudo_maxima      = 0;
    Naturalis numerus_recordi     = 1;
    Naturalis altitudo_recordi    = 1;
    Naturalis numerus_altitudinis = 1;

    printf("Recordi longitudinum:\n");
    for (Naturalis n = 2; n <= limes; n++) {
        Integer longitudo  = longitudo_sequentiae(n);
        Naturalis altitudo = altitudo_maxima(n);

        /* Novus recordus longitudinis */
        if (longitudo > longitudo_maxima) {
            longitudo_maxima = longitudo;
            numerus_recordi  = n;

            /*
             * Promotio typorum: %llu pro Naturalis (unsigned long long),
             * %lld pro Integer (long long) — compilator debet
             * concordantiam format specifiers verificare.
             */
            printf(
                "  n = %6llu: %3lld passus [%s]\n",
                n, longitudo, classifica_numerum(n)
            );
        }

        /* Novus recordus altitudinis */
        if (altitudo > altitudo_recordi) {
            altitudo_recordi    = altitudo;
            numerus_altitudinis = n;
        }
    }

    printf(
        "\nRecordus absolutus longitudinis: n=%llu, %lld passus\n",
        numerus_recordi, longitudo_maxima
    );
    printf(
        "Recordus absolutus altitudinis: n=%llu, altitudo=%llu\n",
        numerus_altitudinis, altitudo_recordi
    );

    /* Scribe aliquot sequentias integras */
    printf("\nSequentiae selectae:\n");
    scribe_sequentiam(27);  /* Sequentia famosa — 111 passus */

    /*
     * Operator commatis in initializatione:
     * ultima expressio dat valorem.
     */
    Naturalis specimen = (limes > 100) ? (Naturalis)97 : limes;
    scribe_sequentiam(specimen);

    /* Distributio longitudinum */
    printf("\nDistributio longitudinum (per decades):\n");
    {
        int maxima_classis = (int)(longitudo_maxima / 10) + 1;
        if (maxima_classis > 50)
            maxima_classis = 50;

        int numerositas_brevis = 0;
        int numerositas_longa  = 0;

        for (Naturalis n = 2; n <= limes; n++) {
            Integer lon = longitudo_sequentiae(n);
            if (lon < 10)
                numerositas_brevis++;
            else if (lon > 100)
                numerositas_longa++;
        }

        printf(
            "  Sequentiae breves (< 10 passus): %d\n",
            numerositas_brevis
        );
        printf(
            "  Sequentiae longae (> 100 passus): %d\n",
            numerositas_longa
        );
    }

    printf("\nConiectura Collatzii verificata usque ad %llu.\n", limes);
    return 0;
}
