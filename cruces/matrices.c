/*
 * matrices.c — Numeri Fibonacci per Exponentiam Matricum
 *
 * Computat F(n) per matricem [[1,1],[1,0]]^n in arithmetica modulari.
 * Cruciatus compilatoris: 'restrict' in parametris, 'static inline',
 * qualificatio 'const' complexa, <inttypes.h> cum PRId64 et INT64_C
 * (concatenatio litteralium textuum cum macris), int64_t.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

/* INT64_C: macro <stdint.h> pro constantibus int64_t */
#define MODULUS INT64_C(1000000007)

typedef int64_t Numerus;

typedef struct {
    Numerus elementa[2][2];
} Matricula;

/* Matriculae constantes cum initializatione plene bracchiata */
static const Matricula MATRICULA_IDENTITATIS = { { {1, 0}, {0, 1} } };
static const Matricula MATRICULA_FIBONACCI   = { { {1, 1}, {1, 0} } };

/*
 * 'static inline' cum 'restrict' — optimizatio maxima.
 * 'restrict' in 'productum' promittit: nulla aliasatio
 * cum 'sinistra' vel 'dextra'.
 * 'const' in 'sinistra' et 'dextra': non mutantur.
 */
static inline void
multiplica(
    Matricula * restrict productum,
    const Matricula *sinistra,
    const Matricula *dextra,
    Numerus modulus
) {
    Matricula temporale = { { {0, 0}, {0, 0} } };
    for (int i = 0; i < 2; i++) {
        for (int k = 0; k < 2; k++) {
            for (int j = 0; j < 2; j++) {
                temporale.elementa[i][k] +=
                    (
                        sinistra->elementa[i][j] *
                        dextra->elementa[j][k]
                    ) % modulus;
            }
            temporale.elementa[i][k] %= modulus;
        }
    }
    *productum = temporale;
}

/*
 * Exponentatio per quadraturam iteratam.
 * 'restrict' in 'eventus' — compilator potest
 * scriptiones ad eventus optimizare.
 */
static void
potentia(
    Matricula * restrict eventus,
    const Matricula *basis,
    Numerus exponens,
    Numerus modulus
) {
    Matricula accumulatum = MATRICULA_IDENTITATIS;
    Matricula base_copia  = *basis;
    Matricula temporale;

    while (exponens > 0) {
        if (exponens & 1) {
            multiplica(&temporale, &accumulatum, &base_copia, modulus);
            accumulatum = temporale;
        }
        multiplica(&temporale, &base_copia, &base_copia, modulus);
        base_copia = temporale;
        exponens >>= 1;
    }

    *eventus = accumulatum;
}

/* Fibonacci per matricem */
static inline Numerus
fibonacci_matricialis(Numerus n, Numerus modulus)
{
    if (n <= 0)
        return 0;
    if (n <= 2)
        return 1;

    Matricula eventus;
    potentia(&eventus, &MATRICULA_FIBONACCI, n - 1, modulus);
    return eventus.elementa[0][0];
}

/* Fibonacci iterativa pro verificatione */
static Numerus
fibonacci_iterativa(Numerus n, Numerus modulus)
{
    if (n <= 0)
        return 0;
    if (n <= 2)
        return 1;

    Numerus prior    = 1;
    Numerus praesens = 1;
    for (Numerus i = 3; i <= n; i++) {
        Numerus proximus = (prior + praesens) % modulus;
        prior = praesens;
        praesens = proximus;
    }
    return praesens;
}

#define LIMES_PRAEDEFINITUM 50

int
main(int argc, char *argv[])
{
    Numerus limes = LIMES_PRAEDEFINITUM;
    if (argc > 1) {
        char *finis;
        long long valor = strtoll(argv[1], &finis, 10);
        if (*finis != '\0' || valor < 1 || valor > 1000000) {
            fprintf(stderr, "Error: argumentum invalidum\n");
            return 1;
        }
        limes = (Numerus)valor;
    }

    /*
     * PRId64: macro <inttypes.h> expanditur ad "lld" vel "ld".
     * Concatenatio litteralium textuum cum macro —
     * cruciatus praprocessoris.
     */
    printf("Numeri Fibonacci (mod %" PRId64 "):\n\n", MODULUS);

    int errores = 0;
    for (Numerus n = 0; n <= limes; n++) {
        Numerus rapida = fibonacci_matricialis(n, MODULUS);
        Numerus lenta  = fibonacci_iterativa(n, MODULUS);

        printf("  F(%2" PRId64 ") = %" PRId64, n, rapida);
        if (n % 5 == 4 || n == limes)
            putchar('\n');

        if (rapida != lenta) {
            printf(
                "    DISCREPANTIA: %" PRId64 " != %" PRId64 "\n",
                rapida, lenta
            );
            errores++;
        }
    }

    /*
     * Verificatio identitatis Fibonacci:
     * F(m+n) = F(m)*F(n+1) + F(m-1)*F(n)
     */
    printf("\nVerificatio identitatis F(m+n) = F(m)*F(n+1) + F(m-1)*F(n):\n");
    for (Numerus m = 5; m <= 20; m++) {
        for (Numerus n = 5; n <= 20; n++) {
            Numerus sinistrum = fibonacci_matricialis(m + n, MODULUS);
            Numerus dextrum =
                (
                    fibonacci_matricialis(m, MODULUS) *
                    fibonacci_matricialis(n + 1, MODULUS) +
                    fibonacci_matricialis(m - 1, MODULUS) *
                    fibonacci_matricialis(n, MODULUS)
                ) % MODULUS;
            if (sinistrum != dextrum) {
                printf("  FALLACIA: m=%" PRId64 " n=%" PRId64 "\n", m, n);
                errores++;
            }
        }
    }
    printf("  Identitas verificata pro m,n in [5,20].\n");

    /*
     * Numeri Fibonacci magni — demonstratio velocitatis matricialis.
     */
    printf("\nNumeri Fibonacci magni (mod %" PRId64 "):\n", MODULUS);
    static const Numerus indices_magni[] = {
        100, 1000, 10000, 100000, 1000000
    };
    for (int i = 0; i < (int)(sizeof(indices_magni) / sizeof(indices_magni[0])); i++) {
        Numerus n_magnus = indices_magni[i];
        if (n_magnus > limes * 100)
            break;
        Numerus valor = fibonacci_matricialis(n_magnus, MODULUS);
        printf("  F(%" PRId64 ") = %" PRId64 "\n", n_magnus, valor);
    }

    if (errores > 0) {
        fprintf(stderr, "Error: %d discrepantiae inventae\n", errores);
        return 1;
    }

    printf("\nOmnes verificiones perfectae sunt.\n");
    return 0;
}
