/*
 * divisores.c — Functiones Divisorum et Identitates Arithmeticae
 *
 * Computat sigma_k(n) pro variis k et identitates verificat.
 * Explorat convolutionem Dirichletianam.
 * Cruciatus compilatoris: expressiones constantes complexae tempore
 * compilationis computatae, enumerationes cum expressionibus computatis,
 * magnitudines arietum staticorum ex expressionibus constantibus,
 * initializatores staticorum non-triviales.
 */

#include <stdio.h>
#include <stdlib.h>

/* ===== Expressiones constantes tempore compilationis ===== */

/*
 * Compilator debet has expressiones tempore compilationis
 * computare — cruciat constantium plicatorem (constant folder).
 */
#define PRIMUS_MAXIMUS 997
#define LIMES_PRAEDEFINITUM 300

/*
 * Enumeratio cum expressionibus constantibus computatis.
 * Compilator debet arithmeticam constantem in enumeratoribus tractare.
 */
enum ConstantesArithmeticae {
    /* Constantia fundamentales */
    UNUM             = 1,
    DUO              = 1 + 1,
    TRIA             = DUO + UNUM,
    SEX              = TRIA * DUO,
    VIGINTI_OCTO     = (1 << 5) - (1 << 2),   /* 32 - 4 = 28 */
    QUADRINGENTI_NONAGINTA_SEX = SEX * 82 + SEX - 2,  /* 496 */

    /* Constantia pro tabulis */
    MAXIMA_MAGNITUDO = LIMES_PRAEDEFINITUM + 1,
    MAXIMUS_EXPONENS = 4,

    /* Expressiones complexae */
    TRIANGULARE_10 = 10 * (10 + 1) / 2,   /* = 55 */
    QUADRATUM_12   = 12 * 12,             /* = 144 */
    CUBUS_6        = 6 * 6 * 6,           /* = 216 */

    /* Operationes bitorum in constantibus */
    MASCHERA_OCTO  = (1 << 8) - 1,        /* 0xFF = 255 */
    MASCHERA_PARVA = MASCHERA_OCTO >> 4,  /* 0x0F = 15 */

    /* Ternarius in constante enumerationis */
    VALOR_CONDITIONALIS = (TRIA > DUO) ? CUBUS_6 : QUADRATUM_12,

    /* sizeof in constante — evaluatur tempore compilationis */
    OCTETI_PER_LONG = sizeof(long),
    OCTETI_PER_INDICATOREM = sizeof(void *),
};

/* ===== Arietes statici cum magnitudine ex expressione constanti ===== */

/* Magnitudo arietis: expressio constans ex enumeratione */
static long tabula_sigma[MAXIMA_MAGNITUDO];
static long tabula_tau[MAXIMA_MAGNITUDO];

/*
 * Initializatores statici non-triviales:
 * Arietes statici cum valoribus computatis ex constantibus.
 */
static const int numeri_perfecti_noti[] = {
    SEX,
    VIGINTI_OCTO,
    QUADRINGENTI_NONAGINTA_SEX,
    8128,
    33550336,
};
#define NUMEROSITAS_PERFECTORUM \
    ((int)(sizeof(numeri_perfecti_noti) / sizeof(numeri_perfecti_noti[0])))

/*
 * Tabula initializata cum expressionibus constantibus.
 */
static const struct {
    const char *nomen;
    int valor;
    int computatum;  /* idem valor, sed per expressionem */
} constantia[] = {
    { "T(10)",     55,  TRIANGULARE_10 },
    { "12^2",     144,  QUADRATUM_12 },
    { "6^3",      216,  CUBUS_6 },
    { "28",        28,  VIGINTI_OCTO },
    { "496",      496,  QUADRINGENTI_NONAGINTA_SEX },
    { "cond",     216,  VALOR_CONDITIONALIS },
};

/* ===== Functiones divisorum ===== */

/* sigma_0(n) = tau(n) = numerositas divisorum */
static long
tau(long n)
{
    if (n <= 0)
        return 0;
    long summa = 0;
    for (long d = 1; d * d <= n; d++) {
        if (n % d == 0) {
            summa++;
            if (d != n / d)
                summa++;
        }
    }
    return summa;
}

/* sigma_k(n) = summa d^k pro d|n */
static long
sigma_k(long n, int k)
{
    if (n <= 0)
        return 0;
    long summa = 0;
    for (long d = 1; d * d <= n; d++) {
        if (n % d == 0) {
            long pot = 1;
            for (int i = 0; i < k; i++)
                pot *= d;
            summa += pot;
            if (d != n / d) {
                long pot2 = 1;
                long d2   = n / d;
                for (int i = 0; i < k; i++)
                    pot2 *= d2;
                summa += pot2;
            }
        }
    }
    return summa;
}

/* sigma_1(n) = sigma(n) = summa divisorum */
static long
sigma(long n)
{
    return sigma_k(n, 1);
}

/* ===== Convolutio Dirichletiana ===== */

/*
 * (f * g)(n) = summa f(d) * g(n/d) pro d|n
 * Demonstrat functiones ut argumenta cum tabulis.
 */
typedef long (*FunctioArithmetica)(long);

static long
convolutio(long n, FunctioArithmetica f, FunctioArithmetica g)
{
    long summa = 0;
    for (long d = 1; d * d <= n; d++) {
        if (n % d == 0) {
            summa += f(d) * g(n / d);
            if (d != n / d)
                summa += f(n / d) * g(d);
        }
    }
    return summa;
}

/* Functio unitatis: u(n) = 1 pro omni n */
static long
unitas(long n)
{
    (void)n;
    return 1;
}

/* Functio identitatis: id(n) = n */
static long
identitas(long n)
{
    return n;
}

int
main(int argc, char *argv[])
{
    int limes = LIMES_PRAEDEFINITUM;
    if (argc > 1) {
        char *finis;
        long valor = strtol(argv[1], &finis, 10);
        if (*finis != '\0' || valor < 1 || valor > LIMES_PRAEDEFINITUM) {
            fprintf(stderr, "Error: argumentum invalidum\n");
            return 1;
        }
        limes = (int)valor;
    }

    /* Verificatio constantium compilatarum */
    printf("Constantia compilatae:\n");
    printf("  sizeof(long) = %d octeti\n", (int)OCTETI_PER_LONG);
    printf("  sizeof(void*) = %d octeti\n", (int)OCTETI_PER_INDICATOREM);
    int errores = 0;
    for (int i = 0; i < (int)(sizeof(constantia) / sizeof(constantia[0])); i++) {
        printf(
            "  %s: %d %s %d\n",
            constantia[i].nomen,
            constantia[i].valor,
            (constantia[i].valor == constantia[i].computatum) ? "==" : "!=",
            constantia[i].computatum
        );
        if (constantia[i].valor != constantia[i].computatum)
            errores++;
    }

    /* Computa tabulas */
    for (int n = 1; n <= limes; n++) {
        tabula_sigma[n] = sigma(n);
        tabula_tau[n]   = tau(n);
    }

    /* Scribe valorem sigma et tau */
    printf("\nFunctiones divisorum pro n = 1..%d:\n", (limes < 30) ? limes : 30);
    for (int n = 1; n <= limes && n <= 30; n++) {
        printf(
            "  sigma(%2d) = %4ld, tau(%2d) = %2ld\n",
            n, tabula_sigma[n], n, tabula_tau[n]
        );
    }

    /* Verificatio: sigma = id * u (convolutio Dirichletiana) */
    printf("\nVerificatio: sigma(n) = (id * u)(n):\n");
    for (int n = 1; n <= limes; n++) {
        long conv = convolutio(n, identitas, unitas);
        if (conv != tabula_sigma[n]) {
            printf(
                "  FALLACIA: sigma(%d) = %ld, (id*u)(%d) = %ld\n",
                n, tabula_sigma[n], n, conv
            );
            errores++;
        }
    }

    /* Verificatio: tau = u * u */
    printf("Verificatio: tau(n) = (u * u)(n):\n");
    for (int n = 1; n <= limes; n++) {
        long conv = convolutio(n, unitas, unitas);
        if (conv != tabula_tau[n]) {
            printf(
                "  FALLACIA: tau(%d) = %ld, (u*u)(%d) = %ld\n",
                n, tabula_tau[n], n, conv
            );
            errores++;
        }
    }

    /* Numeri perfecti noti */
    printf("\nNumeri perfecti noti et verificatio:\n");
    for (int i = 0; i < NUMEROSITAS_PERFECTORUM; i++) {
        long n = numeri_perfecti_noti[i];
        if (n > 100000) {
            printf("  %ld: (nimis magnus pro verificatione hic)\n", n);
            continue;
        }
        long s = sigma(n);
        printf(
            "  sigma(%ld) = %ld %s 2*%ld = %ld\n",
            n, s, (s == 2 * n) ? "==" : "!=", n, 2 * n
        );
        if (s != 2 * n)
            errores++;
    }

    if (errores > 0) {
        fprintf(stderr, "Error: %d verificiones falluerunt\n", errores);
        return 1;
    }

    printf("\nOmnes verificiones perfectae sunt.\n");
    return 0;
}
