/*
 * perfecti.c — Numeri Perfecti, Abundantes, et Deficientes
 *
 * Classificat numeros per summam divisorum propriorum et
 * numeros amicabiles invenit.
 * Cruciatus compilatoris: typedef functionum indicatarum (function
 * pointers), arietes functionum indicatarum, structurae cum
 * functionibus indicatis ut membra, callback per indicatorem.
 */

#include <stdio.h>
#include <stdlib.h>

/* ===== Typi functionum indicatarum ===== */

/* Gradus 1: typedef simplex functionis indicatae */
typedef long (*FunctioDivisorum)(long);

/* Gradus 2: typedef functionis quae functionem indicatam redit */
typedef FunctioDivisorum (*SelectorFunctionis)(int);

/* Gradus 3: structura cum functionibus indicatis ut membris */
typedef struct {
    const char *nomen;
    FunctioDivisorum functio;
    const char *descriptio;
} MethodusDivisorum;

/* ===== Functiones divisorum ===== */

/* sigma(n) — summa omnium divisorum */
static long
summa_divisorum(long n)
{
    if (n <= 0)
        return 0;
    long summa = 0;
    for (long d = 1; d * d <= n; d++) {
        if (n % d == 0) {
            summa += d;
            if (d != n / d)
                summa += n / d;
        }
    }
    return summa;
}

/* s(n) — summa divisorum propriorum (sine n) */
static long
summa_propriorum(long n)
{
    return summa_divisorum(n) - n;
}

/* d(n) — numerositas divisorum */
static long
numerositas_divisorum(long n)
{
    if (n <= 0)
        return 0;
    long numerositas = 0;
    for (long d = 1; d * d <= n; d++) {
        if (n % d == 0) {
            numerositas++;
            if (d != n / d)
                numerositas++;
        }
    }
    return numerositas;
}

/* D(n) — maximus divisor proprius */
static long
maximus_divisor(long n)
{
    if (n <= 1)
        return 0;
    for (long d = 2; d * d <= n; d++) {
        if (n % d == 0)
            return n / d;
    }
    return 1;  /* n est primus */
}

/* ===== Tabula methodorum per arietes functionum indicatarum ===== */

/*
 * Aries functionum indicatarum — compilator debet typos
 * functionum indicatarum in initializatione arietis tractare.
 */
static const MethodusDivisorum methodi[] = {
    { "sigma",  summa_divisorum,       "summa omnium divisorum" },
    { "s",      summa_propriorum,      "summa divisorum propriorum" },
    { "d",      numerositas_divisorum, "numerositas divisorum" },
    { "D",      maximus_divisor,       "maximus divisor proprius" },
};

#define NUMEROSITAS_METHODORUM \
    ((int)(sizeof(methodi) / sizeof(methodi[0])))

/* ===== Selector: functio quae functionem indicatam redit ===== */

/*
 * Redit functionem indicatam per indicem —
 * cruciatus: functio quae functionem indicatam redit.
 */
static FunctioDivisorum
selige_methodum(int index)
{
    if (index < 0 || index >= NUMEROSITAS_METHODORUM)
        return NULL;
    return methodi[index].functio;
}

/* ===== Classificatio per callback ===== */

typedef enum {
    CLASSIS_DEFICIENS = -1,
    CLASSIS_PERFECTUS = 0,
    CLASSIS_ABUNDANS  = 1
} ClassisNumeri;

typedef struct {
    ClassisNumeri classis;
    const char *nomen;
} DescriptioClassis;

static const DescriptioClassis descriptiones_classium[] = {
    { CLASSIS_DEFICIENS, "deficiens" },
    { CLASSIS_PERFECTUS, "perfectus" },
    { CLASSIS_ABUNDANS,  "abundans"  },
};

/*
 * Classificat numerum per functionem indicatam (callback).
 * 'computator' est functio indicata transmissa ut argumentum.
 */
static ClassisNumeri
classifica_per_callback(long n, FunctioDivisorum computator)
{
    long s = computator(n) - n;  /* summa propriorum ex sigma */
    if (s < n)
        return CLASSIS_DEFICIENS;
    if (s > n)
        return CLASSIS_ABUNDANS;
    return CLASSIS_PERFECTUS;
}

/* Nomen classis per indicem */
static const char *
    nomen_classis(ClassisNumeri classis)
{
    for (
        int i = 0;
        i < (int)(sizeof(descriptiones_classium) / sizeof(descriptiones_classium[0]));
        i++
    ) {
        if (descriptiones_classium[i].classis == classis)
            return descriptiones_classium[i].nomen;
    }
    return "ignotus";
}

#define LIMES_PRAEDEFINITUM 10000

int
main(int argc, char *argv[])
{
    long limes = LIMES_PRAEDEFINITUM;
    if (argc > 1) {
        char *finis;
        long valor = strtol(argv[1], &finis, 10);
        if (*finis != '\0' || valor < 1 || valor > 1000000) {
            fprintf(stderr, "Error: argumentum invalidum\n");
            return 1;
        }
        limes = valor;
    }

    /* Demonstratio tabulae methodorum */
    printf("Methodi divisorum (per arietes functionum indicatarum):\n");
    for (int i = 0; i < NUMEROSITAS_METHODORUM; i++) {
        printf(
            "  %s(12) = %ld — %s\n",
            methodi[i].nomen,
            methodi[i].functio(12),
            methodi[i].descriptio
        );
    }

    /* Demonstratio selectoris */
    SelectorFunctionis selector = selige_methodum;
    printf(
        "\nPer selectorem: sigma(28) = %ld\n",
        selector(0)(28)
    );

    /* Inveni numeros perfectos */
    printf("\nNumeri perfecti usque ad %ld:\n", limes);
    int perfecti_inventi = 0;
    for (long n = 2; n <= limes; n++) {
        if (classifica_per_callback(n, summa_divisorum) == CLASSIS_PERFECTUS) {
            printf("  %ld = ", n);
            /* Scribe divisores proprios */
            int primus_divisor = 1;
            for (long d = 1; d < n; d++) {
                if (n % d == 0) {
                    if (!primus_divisor)
                        printf(" + ");
                    printf("%ld", d);
                    primus_divisor = 0;
                }
            }
            putchar('\n');
            perfecti_inventi++;
        }
    }
    printf("Numerositas perfectorum: %d\n", perfecti_inventi);

    /* Inveni numeros amicabiles */
    printf("\nNumeri amicabiles usque ad %ld:\n", limes);
    FunctioDivisorum s     = selige_methodum(1);
    int amicabiles_inventi = 0;
    for (long a = 2; a <= limes; a++) {
        long b = s(a);
        if (b > a && b <= limes && s(b) == a) {
            printf("  (%ld, %ld)\n", a, b);
            amicabiles_inventi++;
        }
    }
    printf("Paria amicabilium: %d\n", amicabiles_inventi);

    /* Statisticae classium — nomen_classis per callback invocatur */
    printf(
        "\nDistributio classium (n = 2..%ld):\n",
        (limes < 1000) ? limes : 1000L
    );
    int census[3] = {0, 0, 0};
    long limes_census = (limes < 1000) ? limes : 1000;
    for (long n = 2; n <= limes_census; n++) {
        ClassisNumeri cl = classifica_per_callback(n, summa_divisorum);
        census[cl + 1]++;
    }
    for (int i = -1; i <= 1; i++) {
        printf("  %-12s %d\n", nomen_classis((ClassisNumeri)i), census[i + 1]);
    }

    printf("\nComputatio perfecta.\n");
    return 0;
}
