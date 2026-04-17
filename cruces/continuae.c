/*
 * catena.c — Fractiones Continuae
 *
 * Expandit radices quadratas in fractiones continuae periodicas
 * et convergentes computat.
 * Cruciatus compilatoris: uniones (unions) pro typo punning,
 * sizeof/offsetof in unionibus, uniones cum structuris et
 * arietibus ut membris, initializatio unionum.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>  /* offsetof */
#include <string.h>

/* ===== Uniones pro repraesentatione numerorum ===== */

/*
 * Unio: fractio vel integer vel floating-point.
 * sizeof(unio) = sizeof(maximi membri).
 * Compilator debet dispositionem et magnitudinem tractare.
 */
typedef struct {
    long numerator;
    long denominator;
} Fractio;

typedef enum {
    TYPUS_INTEGER,
    TYPUS_FRACTIONIS,
    TYPUS_APPROXIMATIONIS
} TypusNumeri;

typedef union {
    long integer;
    Fractio fractio;
    double approximatio;
} ValorNumericus;

/*
 * Numerus discriminatus (tagged union):
 * typus indicat quale membrum unionis activum est.
 */
typedef struct {
    TypusNumeri typus;
    ValorNumericus valor;
} NumerusDiscriminatus;

/* ===== Fractiones continuae ===== */

#define MAXIMA_LONGITUDO_CATENAE 100

typedef struct {
    long coefficientes[MAXIMA_LONGITUDO_CATENAE];
    int longitudo;
    int periodus_initium;  /* -1 si non periodica */
} FractioContinua;

/*
 * Expandit sqrt(n) in fractionem continuam.
 * sqrt(n) = a0 + 1/(a1 + 1/(a2 + ...))
 * Periodus detecta per repetitionem status (m, d).
 */
static FractioContinua
expande_radicem(long n)
{
    FractioContinua fc;
    memset(&fc, 0, sizeof(fc));
    fc.periodus_initium = -1;

    /* Computa partem integram sqrt(n) */
    long a0 = 0;
    while ((a0 + 1) * (a0 + 1) <= n)
        a0++;

    /* Si n est quadratum perfectum, fractio terminat */
    if (a0 * a0 == n) {
        fc.coefficientes[0] = a0;
        fc.longitudo        = 1;
        return fc;
    }

    fc.coefficientes[0] = a0;
    fc.longitudo        = 1;

    /*
     * Algorithmus per uniones: servamus status (m, d) in unione
     * pro demonstratione typi punning.
     */
    typedef union {
        struct {
            long m;
            long d;
        }componentes;
        long paria[2];
    } StatusAlgorithmi;

    StatusAlgorithmi primus_status;
    long m = 0;
    long d = 1;
    long a = a0;

    /* Computa sequentiam */
    for (int i = 1; i < MAXIMA_LONGITUDO_CATENAE; i++) {
        m = d * a - m;
        d = (n - m * m) / d;
        if (d == 0)
            break;
        a = (a0 + m) / d;

        if (i == 1) {
            /* Serva primum statum post a0 */
            primus_status.componentes.m = m;
            primus_status.componentes.d = d;
        } else if (
            primus_status.paria[0] == m &&
            primus_status.paria[1] == d
        ) {
            /* Periodus detecta per accessum arietis unionis */
            fc.periodus_initium = 1;
            break;
        }

        fc.coefficientes[fc.longitudo++] = a;
    }

    return fc;
}

/* ===== Convergentes ===== */

/*
 * Computat convergentes (approximationes rationales)
 * per recurrentiam: h_n/k_n
 */
static NumerusDiscriminatus
computa_convergentem(const FractioContinua *fc, int ordo)
{
    NumerusDiscriminatus eventus;

    if (ordo < 0 || ordo >= fc->longitudo) {
        eventus.typus         = TYPUS_INTEGER;
        eventus.valor.integer = 0;
        return eventus;
    }

    long h_prior    = 1;
    long h_praesens = fc->coefficientes[0];
    long k_prior    = 0;
    long k_praesens = 1;

    for (int i = 1; i <= ordo; i++) {
        long a       = fc->coefficientes[i];
        long h_novum = a * h_praesens + h_prior;
        long k_novum = a * k_praesens + k_prior;
        h_prior      = h_praesens;
        h_praesens   = h_novum;
        k_prior      = k_praesens;
        k_praesens   = k_novum;
    }

    if (k_praesens == 1) {
        eventus.typus         = TYPUS_INTEGER;
        eventus.valor.integer = h_praesens;
    } else {
        eventus.typus = TYPUS_FRACTIONIS;
        eventus.valor.fractio.numerator = h_praesens;
        eventus.valor.fractio.denominator = k_praesens;
    }

    return eventus;
}

/* ===== Functiones auxiliares ===== */

static void
scribe_numerum(NumerusDiscriminatus num)
{
    switch (num.typus) {
    case TYPUS_INTEGER:
        printf("%ld", num.valor.integer);
        break;
    case TYPUS_FRACTIONIS:
        printf(
            "%ld/%ld", num.valor.fractio.numerator,
            num.valor.fractio.denominator
        );
        break;
    case TYPUS_APPROXIMATIONIS:
        printf("%.10f", num.valor.approximatio);
        break;
    }
}

static void
scribe_fractionem_continuam(const FractioContinua *fc)
{
    printf("[%ld", fc->coefficientes[0]);
    if (fc->longitudo > 1) {
        printf("; ");
        int periodus = fc->periodus_initium;
        if (periodus >= 0)
            printf("(");
        for (int i = 1; i < fc->longitudo; i++) {
            if (i > 1)
                printf(", ");
            printf("%ld", fc->coefficientes[i]);
        }
        if (periodus >= 0)
            printf(")");
    }
    printf("]");
}

static long
mcd(long a, long b)
{
    while (b != 0) {
        long t = b;
        b      = a % b;
        a      = t;
    }
    return a;
}

int
main(void)
{
    /* Informatio de unionibus */
    printf("Informationes de unionibus:\n");
    printf("  sizeof(Fractio) = %zu\n", sizeof(Fractio));
    printf(
        "  sizeof(ValorNumericus) = %zu (= max membrorum)\n",
        sizeof(ValorNumericus)
    );
    printf(
        "  sizeof(NumerusDiscriminatus) = %zu\n",
        sizeof(NumerusDiscriminatus)
    );
    printf(
        "  offsetof(ValorNumericus in NumerusDiscriminatus) = %zu\n",
        offsetof(NumerusDiscriminatus, valor)
    );
    printf(
        "  offsetof(fractio.numerator) = %zu\n",
        offsetof(Fractio, numerator)
    );
    printf(
        "  offsetof(fractio.denominator) = %zu\n\n",
        offsetof(Fractio, denominator)
    );

    int errores = 0;

    /* Expande sqrt(n) pro variis n */
    printf("Fractiones continuae radicum quadratarum:\n\n");
    static const int numeri[] = {2, 3, 5, 7, 11, 13, 14, 19, 23, 29, 31};
    for (int idx = 0; idx < (int)(sizeof(numeri) / sizeof(numeri[0])); idx++) {
        int n = numeri[idx];
        FractioContinua fc = expande_radicem(n);

        printf("sqrt(%d) = ", n);
        scribe_fractionem_continuam(&fc);
        printf(
            "  (longitudo periodi: %d)\n",
            (fc.periodus_initium >= 0) ? fc.longitudo - 1 : 0
        );

        /* Convergentes */
        printf("  Convergentes: ");
        int max_ordo = (fc.longitudo < 8) ? fc.longitudo : 8;
        for (int ord = 0; ord < max_ordo; ord++) {
            NumerusDiscriminatus conv = computa_convergentem(&fc, ord);
            if (ord > 0)
                printf(", ");
            scribe_numerum(conv);
        }
        putchar('\n');

        /*
         * Verificatio aequationis Pellianae: h^2 - n*k^2 = (-1)^r
         * ubi r = longitudo periodi, et convergens est p_{r-1}/q_{r-1}
         * (index = r - 1 in coefficientibus post a_0, ergo ordo = r - 1
         *  in arietis coefficientium ubi index 0 = a_0).
         * Ergo ordo convergentis = (fc.longitudo - 1) - 1 = fc.longitudo - 2.
         */
        if (fc.longitudo > 1) {
            int ordo_pell = fc.longitudo - 2;
            NumerusDiscriminatus conv = computa_convergentem(&fc, ordo_pell);
            long h, k;
            if (conv.typus == TYPUS_FRACTIONIS) {
                h = conv.valor.fractio.numerator;
                k = conv.valor.fractio.denominator;
            } else {
                h = conv.valor.integer;
                k = 1;
            }
            long pell = h * h - (long)n * k * k;
            printf("  Pell: %ld^2 - %d*%ld^2 = %ld", h, n, k, pell);
            if (pell != 1 && pell != -1) {
                printf(" (NON +/-1!)");
                errores++;
            }
            putchar('\n');
        }
        putchar('\n');
    }

    /* Fractio continua pro rationali: demonstratio terminationis */
    printf("Fractiones continuae rationalium (algorithmus Euclideus):\n");
    static const Fractio rationales[] = {
        {355, 113},  /* approximatio pi */
        {22, 7},     /* approximatio pi */
        {17, 12},    /* approximatio sqrt(2) */
    };
    for (int i = 0; i < (int)(sizeof(rationales) / sizeof(rationales[0])); i++) {
        long num = rationales[i].numerator;
        long den = rationales[i].denominator;

        printf("  %ld/%ld = [", num, den);
        long a = num;
        long b = den;
        int primus_coeff = 1;
        while (b != 0) {
            if (!primus_coeff)
                printf(", ");
            printf("%ld", a / b);
            primus_coeff = 0;
            long temp = b;
            b = a % b;
            a = temp;
        }
        printf("]");

        /* Verificatio per MCD */
        long d = mcd(num, den);
        printf("  (MCD = %ld)\n", d);
    }

    if (errores > 0) {
        fprintf(stderr, "\nError: %d verificiones falluerunt\n", errores);
        return 1;
    }

    printf("\nOmnes computationes perfectae sunt.\n");
    return 0;
}
