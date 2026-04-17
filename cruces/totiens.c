/*
 * totiens.c — Functio Totiens Euleri
 *
 * Computat phi(n) duabus methodis et concordantiam verificat.
 * Cruciatus compilatoris: concatenatio symbolorum (##),
 * stringificatio (#), macrae variadicae (__VA_ARGS__),
 * X-macrae ad enumerationes et tabulas generandas,
 * expansio nestificata macrarum.
 */

#include <stdio.h>
#include <stdlib.h>

/* ===== Stratum I: concatenatio et stringificatio ===== */

#define CONIUNGO_IMPL(a, b) a##b
#define CONIUNGO(a, b) CONIUNGO_IMPL(a, b)

#define TEXTUS_IMPL(x) #x
#define TEXTUS(x) TEXTUS_IMPL(x)

/* ===== Stratum II: macrae variadicae ===== */

#define SCRIBE(forma, ...) \
    printf("[linea " TEXTUS(__LINE__) "] " forma "\n", __VA_ARGS__)

#define SCRIBE_SI(conditio, forma, ...) \
    do { if (conditio) printf(forma "\n", __VA_ARGS__); } while (0)

#define AFFIRMA(conditio, nuntium) \
    do { if (!(conditio)) { \
        fprintf(stderr, "Affirmatio fallit [%s:%d]: %s\n", \
                __FILE__, __LINE__, nuntium); \
        return 1; \
    } } while (0)

/* ===== Stratum III: X-macrae ===== */

/*
 * X-macro: lista methodorum computandi.
 * Unica definitio generat enumerationem, tabulas textuales,
 * et tabulam functionum — sine repetitione.
 */
#define METHODI(X) \
    X(directa,  "enumeratio directa coprimorum") \
    X(formulae, "formula producti Euleri")

/* Enumeratio ex X-macro generata */
typedef enum {
#define GENERA_ENUM(nomen, descriptio) METHODUS_##nomen,
    METHODI(GENERA_ENUM)
#undef GENERA_ENUM
    METHODORUM_NUMEROSITAS
} IndexMethodi;

/* Tabula descriptionum cum designatoribus initializationis */
static const char *const descriptiones[] = {
#define GENERA_DESCRIPTIONEM(nomen, descriptio) \
    [METHODUS_##nomen] = descriptio,
    METHODI(GENERA_DESCRIPTIONEM)
#undef GENERA_DESCRIPTIONEM
};

/* ===== Stratum IV: functiones computandi ===== */

static int
totiens_directa(int n)
{
    if (n <= 0)
        return 0;
    int eventus = 0;
    for (int i = 1; i <= n; i++) {
        /* Algorithmus Euclideus pro MCD */
        int a = i;
        int b = n;
        while (b != 0) {
            int t = b;
            b     = a % b;
            a     = t;
        }
        if (a == 1)
            eventus++;
    }
    return eventus;
}

static int
totiens_formulae(int n)
{
    if (n <= 0)
        return 0;
    int eventus = n;
    int m       = n;
    for (int p = 2; p * p <= m; p++) {
        if (m % p == 0) {
            while (m % p == 0)
                m /= p;
            eventus -= eventus / p;
        }
    }
    if (m > 1)
        eventus -= eventus / m;
    return eventus;
}

/* ===== Stratum V: tabula functionum per concatenationem ===== */

typedef int (*FunctioTotiens)(int);

/*
 * CONIUNGO(totiens_, nomen) generat nomen functionis:
 *   totiens_directa, totiens_formulae, etc.
 * TEXTUS(nomen) generat textum: "directa", "formulae", etc.
 */
static const struct {
    FunctioTotiens functio;
    const char *nomen;
} tabula_methodorum[] = {
#define GENERA_TABULAM(nomen, descriptio) \
    { CONIUNGO(totiens_, nomen), TEXTUS(nomen) },
    METHODI(GENERA_TABULAM)
#undef GENERA_TABULAM
};

/* Macro quae methodum nominatim invocat */
#define INVOCA(methodus, n) CONIUNGO(totiens_, methodus)(n)

#define LIMES_PRAEDEFINITUM 80

int
main(int argc, char *argv[])
{
    int limes = LIMES_PRAEDEFINITUM;
    if (argc > 1) {
        char *finis;
        long valor = strtol(argv[1], &finis, 10);
        if (*finis != '\0' || valor < 1 || valor > 10000) {
            fprintf(stderr, "Error: argumentum invalidum\n");
            return 1;
        }
        limes = (int)valor;
    }

    /* Demonstratio X-macro: scribe methodos ex tabula generata */
    printf("Methodi computandi functionem totientem:\n");
    for (int i = 0; i < (int)METHODORUM_NUMEROSITAS; i++)
        printf(
            "  %d. %s: %s\n", i + 1,
            tabula_methodorum[i].nomen, descriptiones[i]
        );

    /* Computa phi(n) utraque methodo */
    int errores = 0;
    printf("\nFunctio totiens phi(n), n = 1..%d:\n", limes);
    for (int n = 1; n <= limes; n++) {
        int phi_d = INVOCA(directa, n);
        int phi_f = INVOCA(formulae, n);

        printf(" phi(%d)=%d", n, phi_d);
        if (n % 10 == 0)
            putchar('\n');

        if (phi_d != phi_f) {
            SCRIBE("Discrepantia: phi(%d) = %d vs %d", n, phi_d, phi_f);
            errores++;
        }
    }
    if (limes % 10 != 0)
        putchar('\n');

    /* Verificatio theorematis: summa phi(d) pro d|n = n */
    printf("\nVerificatio: summa phi(d) pro d|n debet aequare n\n");
    for (int n = 1; n <= limes; n++) {
        int summa = 0;
        for (int d = 1; d <= n; d++) {
            if (n % d == 0)
                summa += tabula_methodorum[0].functio(d);
        }
        SCRIBE_SI(
            summa != n,
            "  FALLACIA: summa phi(d|%d) = %d, non %d",
            n, summa, n
        );
        if (summa != n)
            errores++;
    }

    if (errores > 0) {
        fprintf(stderr, "Error: %d verificiones falluerunt\n", errores);
        return 1;
    }

    printf("Omnes verificiones perfectae sunt.\n");
    return 0;
}
