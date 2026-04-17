/*
 * moebiana.c — Functio Moebii et Numeri Liberi Quadratis
 *
 * Computat mu(n) et proprietates arithmeticas numerorum.
 * Cruciatus compilatoris: campi bitorum (bitfields) cum latitudinibus
 * variis et signis mixtis, sizeof structurae cum campis bitorum,
 * _Bool, enumerationes cum valoribus negativis.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ===== Enumeratio cum valoribus negativis ===== */

typedef enum {
    VALOR_MU_NEGATIVUS = -1,
    VALOR_MU_NULLUS    =  0,
    VALOR_MU_POSITIVUS =  1
} ValorMoebii;

/* ===== Campi bitorum: latitudines et signa varia ===== */

/*
 * Structura cum campis bitorum mixtis:
 * - 'unsigned' campi pro vexillis boolianis
 * - 'signed int' campus pro valore mu (-1, 0, +1)
 * - latitudines variae: 1, 2, 4, 5 bites
 * Compilator debet dispositionem et impletionem recte tractare.
 */
typedef struct {
    unsigned int est_primus          : 1;
    unsigned int est_liber_quadratis : 1;
    signed int   valor_mu            : 2;  /* -1, 0, +1 */
    unsigned int numerositas_factorum: 4;  /* 0..15 factores */
    unsigned int minimus_factor      : 5;  /* primus minimus, usque ad 31 */
} ProprietatesNumerales;

/* ===== Functiones computandi ===== */

static _Bool
est_primus(int n)
{
    if (n < 2)
        return false;
    if (n < 4)
        return true;
    if (n % 2 == 0 || n % 3 == 0)
        return false;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}

/*
 * Computat mu(n) et proprietates simul per factorizationem.
 * Redit structuram cum campis bitorum.
 */
static ProprietatesNumerales
computa_proprietates(int n)
{
    ProprietatesNumerales prop = {0, 0, 0, 0, 0};

    if (n <= 0) {
        prop.valor_mu = VALOR_MU_NULLUS;
        return prop;
    }
    if (n == 1) {
        prop.est_liber_quadratis = 1;
        prop.valor_mu = VALOR_MU_POSITIVUS;
        return prop;
    }

    prop.est_primus = est_primus(n) ? 1 : 0;

    int m = n;
    int factores = 0;
    _Bool liber = true;
    int primus_minimus = 0;

    for (int p = 2; p * p <= m; p++) {
        if (m % p == 0) {
            if (primus_minimus == 0)
                primus_minimus = p;
            factores++;
            m /= p;
            if (m % p == 0) {
                liber = false;
                /* Quadratus dividit n — mu(n) = 0 */
                while (m % p == 0) {
                    m /= p;
                }
            }
        }
    }
    if (m > 1) {
        if (primus_minimus == 0)
            primus_minimus = m;
        factores++;
    }

    prop.est_liber_quadratis  = liber ? 1 : 0;
    prop.numerositas_factorum = (unsigned)factores;
    prop.minimus_factor       = (primus_minimus <= 31) ? (unsigned)primus_minimus : 0;

    if (!liber) {
        prop.valor_mu = VALOR_MU_NULLUS;
    } else if (factores % 2 == 0) {
        prop.valor_mu = VALOR_MU_POSITIVUS;
    } else {
        prop.valor_mu = VALOR_MU_NEGATIVUS;
    }

    return prop;
}

/* Verificatio: summa mu(d) pro d|n = (n == 1 ? 1 : 0) */
static int
verifica_summa_moebii(int limes)
{
    int errores = 0;
    for (int n = 1; n <= limes; n++) {
        int summa = 0;
        for (int d = 1; d <= n; d++) {
            if (n % d == 0) {
                ProprietatesNumerales prop = computa_proprietates(d);
                summa += prop.valor_mu;
            }
        }
        int expectatum = (n == 1) ? 1 : 0;
        if (summa != expectatum) {
            printf(
                "  FALLACIA: summa mu(d|%d) = %d, expectatum %d\n",
                n, summa, expectatum
            );
            errores++;
        }
    }
    return errores;
}

#define LIMES_PRAEDEFINITUM 120

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

    /* sizeof structurae cum campis bitorum — dependet ab impletione */
    printf(
        "Magnitudo ProprietatesNumerales: %zu octeti\n",
        sizeof(ProprietatesNumerales)
    );
    printf("Magnitudo _Bool: %zu octeti\n\n", sizeof(_Bool));

    printf("Functio Moebii mu(n) pro n = 1..%d:\n", limes);
    int primos  = 0;
    int liberos = 0;
    for (int n = 1; n <= limes; n++) {
        ProprietatesNumerales prop = computa_proprietates(n);

        if (n <= 60) {
            printf(" mu(%2d)=%+d", n, (int)prop.valor_mu);
            if (n % 10 == 0)
                putchar('\n');
        }

        if (prop.est_primus)
            primos++;
        if (prop.est_liber_quadratis)
            liberos++;
    }
    if (limes <= 60 && limes % 10 != 0)
        putchar('\n');

    printf("\nStatistica usque ad %d:\n", limes);
    printf("  Numeri primi: %d\n", primos);
    printf("  Numeri liberi quadratis: %d\n", liberos);

    /* Densitas numerorum liberorum quadratis -> 6/pi^2 ~ 0.6079 */
    printf(
        "  Densitas liberorum: %.4f (expectata ~0.6079)\n",
        (double)liberos / (double)limes
    );

    /* Verificatio identitatis summae Moebii */
    int limes_verificationis = (limes < 200) ? limes : 200;
    printf(
        "\nVerificatio: summa mu(d) pro d|n (n=1..%d):\n",
        limes_verificationis
    );
    int errores = verifica_summa_moebii(limes_verificationis);

    /* Functio Mertens: M(n) = summa mu(k) pro k=1..n */
    printf("\nFunctio Mertens M(n):\n");
    int mertens = 0;
    for (int n = 1; n <= limes; n++) {
        ProprietatesNumerales prop = computa_proprietates(n);
        mertens += prop.valor_mu;
        if (n % 10 == 0 || n == limes)
            printf("  M(%d) = %d\n", n, mertens);
    }

    if (errores > 0) {
        fprintf(stderr, "Error: %d fallaciae inventae\n", errores);
        return 1;
    }

    printf("\nOmnes verificiones perfectae sunt.\n");
    return 0;
}
