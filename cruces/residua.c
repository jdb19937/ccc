/*
 * residua.c — Theorema Residuorum Sinensium (CRT)
 *
 * Solvit systemata congruentiarum per Theorema Sinensium.
 * Cruciatus compilatoris: membra arietis flexibilia (flexible array
 * members), sizeof structurae cum FAM, offsetof, allocatio
 * dynamica pro FAM, structurae nestificatae.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>  /* offsetof */
#include <string.h>

/* ===== Structurae cum membro flexibili ===== */

/*
 * Congruentia singularis: x = residuum (mod modulus)
 */
typedef struct {
    long residuum;
    long modulus;
} Congruentia;

/*
 * Systema congruentiarum cum membro arietis flexibili (FAM).
 * Ultimum membrum '[]' sine dimensione — C99 FAM.
 * sizeof(SystemaCongruentiarum) NON includit elementa flexibilia.
 */
typedef struct {
    int numerositas;
    char descriptio[64];
    Congruentia congruentiae[];  /* FAM — debet esse ultimum */
} SystemaCongruentiarum;

/* ===== Functiones arithmeticae ===== */

/* Algorithmus Euclideus extensus */
static long
mcd_extensus(long a, long b, long *coefficiens_x, long *coefficiens_y)
{
    if (a == 0) {
        *coefficiens_x = 0;
        *coefficiens_y = 1;
        return b;
    }
    long x1;
    long y1;
    long d         = mcd_extensus(b % a, a, &x1, &y1);
    *coefficiens_x = y1 - (b / a) * x1;
    *coefficiens_y = x1;
    return d;
}

/* Inversum modulare: a^(-1) mod m */
static long
inversum_modulare(long a, long m)
{
    long x;
    long y;
    long d = mcd_extensus(a, m, &x, &y);
    (void)y;  /* non utimur */
    if (d != 1)
        return -1;  /* inversum non existit */
    return ((x % m) + m) % m;
}

/* ===== Allocatio et creatio systematum ===== */

/*
 * Allocat systema cum FAM — demonstrat patternm allocandi:
 * sizeof(structura) + n * sizeof(elementum_flexibile)
 */
static SystemaCongruentiarum *
    crea_systema(
        const char *descriptio, int numerositas,
        const Congruentia *congruentiae
    ) {
    /*
     * offsetof cum FAM: distantia a principio structurae
     * ad initium arietis flexibilis.
     */
    size_t magnitudo = offsetof(SystemaCongruentiarum, congruentiae)
        + (size_t)numerositas * sizeof(Congruentia);

    SystemaCongruentiarum *systema = malloc(magnitudo);
    if (systema == NULL)
        return NULL;

    systema->numerositas = numerositas;
    strncpy(
        systema->descriptio, descriptio,
        sizeof(systema->descriptio) - 1
    );
    systema->descriptio[sizeof(systema->descriptio) - 1] = '\0';

    memcpy(
        systema->congruentiae, congruentiae,
        (size_t)numerositas * sizeof(Congruentia)
    );

    return systema;
}

/* ===== Solutio CRT ===== */

/*
 * Solvit systema congruentiarum per CRT.
 * Redit solutionem x vel -1 si systema insolvibile est.
 */
static long
solve_crt(const SystemaCongruentiarum *systema)
{
    if (systema->numerositas == 0)
        return 0;

    long solutio = systema->congruentiae[0].residuum;
    long modulus_accumulatus = systema->congruentiae[0].modulus;

    for (int i = 1; i < systema->numerositas; i++) {
        long r = systema->congruentiae[i].residuum;
        long m = systema->congruentiae[i].modulus;

        long inv = inversum_modulare(modulus_accumulatus, m);
        if (inv < 0)
            return -1;

        long differentia = ((r - solutio) % m + m) % m;
        solutio = solutio + modulus_accumulatus * ((differentia * inv) % m);
        modulus_accumulatus *= m;
        solutio = ((solutio % modulus_accumulatus) + modulus_accumulatus)
            % modulus_accumulatus;
    }

    return solutio;
}

/* Verificat solutionem contra omnes congruentias */
static int
verifica_solutionem(const SystemaCongruentiarum *systema, long solutio)
{
    int errores = 0;
    for (int i = 0; i < systema->numerositas; i++) {
        long residuum = solutio % systema->congruentiae[i].modulus;
        long expectatum = systema->congruentiae[i].residuum
            % systema->congruentiae[i].modulus;
        if (residuum != expectatum) {
            printf(
                "    FALLACIA: %ld mod %ld = %ld, expectatum %ld\n",
                solutio, systema->congruentiae[i].modulus,
                residuum, expectatum
            );
            errores++;
        }
    }
    return errores;
}

int
main(void)
{
    int errores = 0;

    /* sizeof structurae cum FAM — non includit elementa flexibilia */
    printf(
        "sizeof(SystemaCongruentiarum) = %zu (sine FAM)\n",
        sizeof(SystemaCongruentiarum)
    );
    printf("sizeof(Congruentia) = %zu\n", sizeof(Congruentia));
    printf(
        "offsetof(congruentiae) = %zu\n\n",
        offsetof(SystemaCongruentiarum, congruentiae)
    );

    /*
     * Exemplum classicum: problema Sunzi (III saeculum)
     * x = 2 (mod 3), x = 3 (mod 5), x = 2 (mod 7)
     * Solutio: x = 23
     */
    {
        Congruentia cong[] = { {2, 3}, {3, 5}, {2, 7} };
        SystemaCongruentiarum *sys = crea_systema(
            "Problema Sunzi", 3, cong
        );
        if (sys == NULL) {
            fprintf(stderr, "Error: allocatio memoriae fallit\n");
            return 1;
        }

        printf("=== %s ===\n", sys->descriptio);
        for (int i = 0; i < sys->numerositas; i++) {
            printf(
                "  x = %ld (mod %ld)\n",
                sys->congruentiae[i].residuum,
                sys->congruentiae[i].modulus
            );
        }

        long solutio = solve_crt(sys);
        printf("  Solutio: x = %ld\n", solutio);
        errores += verifica_solutionem(sys, solutio);
        free(sys);
    }

    /*
     * Systema maius: x = 1(mod 2), x = 2(mod 3), x = 3(mod 5),
     *                x = 4(mod 7), x = 5(mod 11)
     */
    {
        Congruentia cong[] = { {1, 2}, {2, 3}, {3, 5}, {4, 7}, {5, 11} };
        SystemaCongruentiarum *sys = crea_systema(
            "Systema quinque congruentiarum", 5, cong
        );
        if (sys == NULL) {
            fprintf(stderr, "Error: allocatio memoriae fallit\n");
            return 1;
        }

        printf("\n=== %s ===\n", sys->descriptio);
        for (int i = 0; i < sys->numerositas; i++) {
            printf(
                "  x = %ld (mod %ld)\n",
                sys->congruentiae[i].residuum,
                sys->congruentiae[i].modulus
            );
        }

        long solutio         = solve_crt(sys);
        long modulus_totalis = 2L * 3 * 5 * 7 * 11;
        printf("  Solutio: x = %ld (mod %ld)\n", solutio, modulus_totalis);
        errores += verifica_solutionem(sys, solutio);
        free(sys);
    }

    /*
     * Genera systema ex numeris primis et solve.
     */
    {
        static const int primi[] = {2, 3, 5, 7, 11, 13, 17, 19, 23};
        int n = (int)(sizeof(primi) / sizeof(primi[0]));
        Congruentia *cong = malloc((size_t)n * sizeof(Congruentia));
        if (cong == NULL) {
            fprintf(stderr, "Error: allocatio memoriae fallit\n");
            return 1;
        }

        long secretum = 12345;
        for (int i = 0; i < n; i++) {
            cong[i].residuum = secretum % primi[i];
            cong[i].modulus  = primi[i];
        }

        SystemaCongruentiarum *sys = crea_systema(
            "Reconstructio numeri secreti", n, cong
        );
        free(cong);
        if (sys == NULL) {
            fprintf(stderr, "Error: allocatio memoriae fallit\n");
            return 1;
        }

        printf("\n=== %s ===\n", sys->descriptio);
        for (int i = 0; i < sys->numerositas; i++) {
            printf(
                "  x = %ld (mod %ld)\n",
                sys->congruentiae[i].residuum,
                sys->congruentiae[i].modulus
            );
        }

        long solutio = solve_crt(sys);
        printf("  Solutio (numerus secretus): x = %ld\n", solutio);
        if (solutio != secretum) {
            printf("  FALLACIA: expectatum %ld\n", secretum);
            errores++;
        }

        errores += verifica_solutionem(sys, solutio);
        free(sys);
    }

    if (errores > 0) {
        fprintf(stderr, "Error: %d fallaciae inventae\n", errores);
        return 1;
    }

    printf("\nOmnes solutiones verificatae sunt.\n");
    return 0;
}
