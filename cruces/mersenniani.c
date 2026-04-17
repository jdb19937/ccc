/*
 * mersenniani.c — Numeri Primi Mersenniani
 *
 * Investigat numeros formae 2^p - 1 per probam Lucas-Lehmer.
 * Explorat proprietates repunitorum in variis basibus.
 * Cruciatus compilatoris: ambitus nestificati cum obumbratione
 * (shadowing) variabilium, blocki nestificati profunde,
 * identici nomini variabilium in ambitibus differentibus,
 * regolae ambitus complexae C99.
 */

#include <stdio.h>
#include <stdlib.h>

/* ===== Functiones arithmeticae fundamentales ===== */

static int
est_primus(int n)
{
    if (n < 2)
        return 0;
    if (n < 4)
        return 1;
    if (n % 2 == 0 || n % 3 == 0)
        return 0;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    }
    return 1;
}

/*
 * Multiplicatio modularis: (a * b) % m
 * Evitat superfluxum per reductionem.
 */
static long long
multiplica_mod(long long a, long long b, long long m)
{
    long long eventus = 0;
    a %= m;
    while (b > 0) {
        if (b & 1)
            eventus = (eventus + a) % m;
        a = (a * 2) % m;
        b >>= 1;
    }
    return eventus;
}

/*
 * Potentia modularis: (basis ^ exponens) % modulus
 */
static long long
potentia_mod(long long basis, long long exponens, long long modulus)
{
    long long eventus = 1;
    basis %= modulus;
    while (exponens > 0) {
        if (exponens & 1)
            eventus = multiplica_mod(eventus, basis, modulus);
        basis = multiplica_mod(basis, basis, modulus);
        exponens >>= 1;
    }
    return eventus;
}

/* ===== Proba Lucas-Lehmer ===== */

/*
 * Verificat an 2^p - 1 sit primus per algorithmum Lucas-Lehmer.
 * Pro p > 2: computat s_i = s_{i-1}^2 - 2 (mod 2^p - 1)
 * cum s_0 = 4. Si s_{p-2} = 0, tunc 2^p - 1 est primus.
 *
 * Obumbratio variabilium: variabilis 'eventus' apparet in
 * pluribus ambitibus nestificatis cum significationibus differentibus.
 */
static int
proba_lucas_lehmer(int p)
{
    /* Ambitus 0: eventus est booleanus */
    int eventus = 0;

    if (p == 2) {
        eventus = 1;  /* 2^2 - 1 = 3 est primus */
        return eventus;
    }

    if (p < 2 || !est_primus(p))
        return 0;

    /* Ambitus 1: mersennianus est numerus sub examine */
    {
        long long mersennianus = (1LL << p) - 1;

        /* Ambitus 2: variabilis 's' pro sequentia Lucas-Lehmer */
        {
            long long s = 4;

            /* Ambitus 3: iteratio p-2 vicibus */
            {
                int iterationes = p - 2;
                for (int i = 0; i < iterationes; i++) {
                    /*
                     * Ambitus 4: computatio s^2 - 2 mod M
                     * 'eventus' hic non obumbratur — accedit ad
                     * variabilem ex ambitu 0.
                     */
                    {
                        long long quadratum = multiplica_mod(s, s, mersennianus);
                        s = quadratum - 2;
                        if (s < 0)
                            s += mersennianus;
                    }
                }
            }

            eventus = (s == 0) ? 1 : 0;
        }
    }

    return eventus;
}

/* ===== Repuniti in basi b: (b^n - 1) / (b - 1) ===== */

/*
 * Computat repunitum et proprietates investigat.
 * Obumbratio profunda: 'n' apparet in pluribus ambitibus.
 */
static void
investiga_repunitos(int basis, int limes_exponens)
{
    printf("\nRepuniti in basi %d:\n", basis);

    for (int n = 2; n <= limes_exponens; n++) {
        if (!est_primus(n))
            continue;

        /* Ambitus: 'valor' est repunitus */
        {
            long long valor = 0;
            long long pot   = 1;

            /* Ambitus interior: 'i' ad computandum repunitum */
            {
                for (int i = 0; i < n; i++) {
                    valor += pot;
                    pot *= basis;
                }
            }

            /* Verificatio primitatis per divisiones probatorias */
            {
                int est_primus_repunitus = 1;

                /* Ambitus: 'i' reusata (obumbratio in novo ambitu for) */
                for (long long i = 2; i * i <= valor; i++) {
                    if (valor % i == 0) {
                        est_primus_repunitus = 0;
                        break;
                    }
                }

                printf(
                    "  R(%d,%d) = %lld %s\n",
                    basis, n, valor,
                    est_primus_repunitus ? "(primus!)" : ""
                );
            }
        }
    }
}

/* ===== Programma principale ===== */

int
main(int argc, char *argv[])
{
    int limes = 31;

    if (argc > 1) {
        char *finis;
        long valor = strtol(argv[1], &finis, 10);
        if (*finis != '\0' || valor < 2 || valor > 61) {
            fprintf(stderr, "Error: argumentum invalidum (2..61)\n");
            return 1;
        }
        limes = (int)valor;
    }

    printf("Numeri Primi Mersenniani (2^p - 1)\n");
    printf("Proba Lucas-Lehmer usque ad p = %d\n\n", limes);

    printf("%-6s  %-20s  %-10s\n", "p", "2^p - 1", "Status");
    printf("%-6s  %-20s  %-10s\n", "------", "--------------------", "----------");

    int mersenniani_inventi = 0;

    /* Ambitus principalis: 'p' est exponens */
    for (int p = 2; p <= limes; p++) {
        if (!est_primus(p))
            continue;

        long long mersennianus = (1LL << p) - 1;

        /* Ambitus: 'eventus' obumbrat nihil hic, sed demonstrat
         * variabilem localem in ambitu for */
        {
            int est_mersennianus_primus = proba_lucas_lehmer(p);
            printf(
                "%-6d  %-20lld  %s\n",
                p, mersennianus,
                est_mersennianus_primus ? "PRIMUS" : "compositus"
            );

            if (est_mersennianus_primus) {
                mersenniani_inventi++;

                /* Numerus perfectus correspondents: 2^(p-1) * (2^p - 1) */
                {
                    long long perfectus = (1LL << (p - 1)) * mersennianus;
                    printf("  -> Numerus perfectus: %lld\n", perfectus);
                }
            }
        }
    }

    printf(
        "\nNumerositas primorum Mersennianorum inventorum: %d\n",
        mersenniani_inventi
    );

    /* Repuniti sunt generalizatio numerorum Mersennianorum */
    investiga_repunitos(2, 19);  /* = numeri Mersenniani */
    investiga_repunitos(10, 19); /* repuniti decimales */
    investiga_repunitos(3, 13);

    /* Proba Fermat per potentia_mod: 2^(n-1) mod n = 1 si n primus */
    printf("\nProba Fermat per exponentiam modularem:\n");
    for (int p = 2; p <= limes && p <= 19; p++) {
        if (!est_primus(p))
            continue;
        long long m      = (1LL << p) - 1;
        long long fermat = potentia_mod(2, m - 1, m);
        printf(
            "  2^(M_%d - 1) mod M_%d = %lld %s\n",
            p, p, fermat,
            (fermat == 1) ? "(Fermat satisfactum)" : "(compositus)"
        );
    }

    /* Coniectatio Lenstra-Pomerance-Wagstaff */
    printf("\nConiectura: numerositas primorum Mersennianorum ");
    printf("usque ad exponentem x est circa (e^gamma / ln 2) * ln(x)\n");
    printf("ubi gamma = constans Euler-Mascheroni ~ 0.5772\n");

    printf("\nComputatio perfecta.\n");
    return 0;
}
