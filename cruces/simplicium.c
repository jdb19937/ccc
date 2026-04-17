/*
 * simplicium.c — Complexus Simpliciales et Numeri Betti
 *
 * Construit complexus simpliciales et facies numerat per dimensionem.
 * Characteristicam Euleri computat ex numeris facierum.
 * Cruciatus compilatoris: functiones variadicae (va_list, va_arg,
 * va_start, va_end), promotiones typorum in argumentis variadicis
 * (char -> int, float -> double), va_copy (C99).
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MAXIMI_VERTICES  16
#define MAXIMA_DIMENSIO   4
#define MAXIMI_SIMPLICES 256

/* ===== Typi ===== */

/*
 * Simplex: coniunctio verticium.
 * Dimensio = numerositas_verticium - 1.
 */
typedef struct {
    int vertices[MAXIMI_VERTICES];
    int numerositas;
} Simplex;

typedef struct {
    Simplex simplices[MAXIMI_SIMPLICES];
    int numerositas;
    int dimensio_maxima;
} ComplexusSimplicialis;

/* ===== Functiones variadicae ===== */

/*
 * Creat simplicem ex lista variadica verticium.
 * Primus argumentum: numerositas verticium.
 * Sequentes: vertices (int) — promotio typorum applicatur.
 */
static Simplex
fac_simplicem(int numerositas, ...)
{
    Simplex s;
    s.numerositas = numerositas;
    va_list argumenta;
    va_start(argumenta, numerositas);

    for (int i = 0; i < numerositas; i++) {
        /* va_arg cum typo 'int' — argumenta iam promota sunt */
        s.vertices[i] = va_arg(argumenta, int);
    }

    va_end(argumenta);

    /* Ordina vertices */
    for (int i = 0; i < numerositas - 1; i++) {
        for (int k = i + 1; k < numerositas; k++) {
            if (s.vertices[i] > s.vertices[k]) {
                int temp      = s.vertices[i];
                s.vertices[i] = s.vertices[k];
                s.vertices[k] = temp;
            }
        }
    }

    return s;
}

/*
 * Functio variadica quae textum format in acervum staticum.
 * Demonstrat va_copy (C99) — copia listae variadicae.
 */
static const char *
    forma_textum(const char *forma, ...)
{
    static char acervus[256];
    va_list argumenta;
    va_list copia;

    va_start(argumenta, forma);

    /* va_copy: creat copiam independentem listae variadicae */
    va_copy(copia, argumenta);

    /* Prima invocatio: computat longitudinem */
    int longitudo = vsnprintf(NULL, 0, forma, argumenta);
    va_end(argumenta);

    /* Secunda invocatio cum copia: scribit textum */
    if (longitudo >= 0 && (size_t)longitudo < sizeof(acervus)) {
        vsnprintf(acervus, sizeof(acervus), forma, copia);
    } else {
        strncpy(acervus, "(textus nimis longus)", sizeof(acervus) - 1);
        acervus[sizeof(acervus) - 1] = '\0';
    }

    va_end(copia);
    return acervus;
}

/*
 * Addit plures simplices ad complexum per functionem variadicam.
 * Ultimus argumentum: simplex cum numerositas = 0 (sentinella).
 */
static void
adde_simplices(ComplexusSimplicialis *complexus, int numerositas_novorum, ...)
{
    va_list argumenta;
    va_start(argumenta, numerositas_novorum);

    for (int i = 0; i < numerositas_novorum; i++) {
        if (complexus->numerositas >= MAXIMI_SIMPLICES)
            break;
        /* va_arg cum typo structurae — promotio non applicatur */
        Simplex s = va_arg(argumenta, Simplex);
        complexus->simplices[complexus->numerositas++] = s;
        int dim = s.numerositas - 1;
        if (dim > complexus->dimensio_maxima)
            complexus->dimensio_maxima = dim;
    }

    va_end(argumenta);
}

/* ===== Operationes in complexu ===== */

static void
initia_complexum(ComplexusSimplicialis *complexus)
{
    complexus->numerositas     = 0;
    complexus->dimensio_maxima = -1;
}

/* Numerat simplices per dimensionem — f-vector */
static void
computa_f_vectorem(
    const ComplexusSimplicialis *complexus,
    int *f_vector, int maxima_dim
) {
    memset(f_vector, 0, (size_t)(maxima_dim + 1) * sizeof(int));
    for (int i = 0; i < complexus->numerositas; i++) {
        int dim = complexus->simplices[i].numerositas - 1;
        if (dim >= 0 && dim <= maxima_dim)
            f_vector[dim]++;
    }
}

/* Characteristica Euleri ex f-vectore: chi = f0 - f1 + f2 - ... */
static int
characteristica_euleri(const int *f_vector, int maxima_dim)
{
    int chi = 0;
    for (int d = 0; d <= maxima_dim; d++) {
        chi += (d % 2 == 0) ? f_vector[d] : -f_vector[d];
    }
    return chi;
}

/* Scribe simplicem */
static void
scribe_simplicem(const Simplex *s)
{
    putchar('{');
    for (int i = 0; i < s->numerositas; i++) {
        if (i > 0)
            printf(", ");
        printf("%d", s->vertices[i]);
    }
    putchar('}');
}

/* ===== Constructio complexuum notorum ===== */

static void
construe_limitem_tetrahedri(ComplexusSimplicialis *complexus)
{
    initia_complexum(complexus);

    /* Vertices (0-simplices) */
    adde_simplices(
        complexus, 4,
        fac_simplicem(1, 0),
        fac_simplicem(1, 1),
        fac_simplicem(1, 2),
        fac_simplicem(1, 3)
    );

    /* Latera (1-simplices) */
    adde_simplices(
        complexus, 6,
        fac_simplicem(2, 0, 1),
        fac_simplicem(2, 0, 2),
        fac_simplicem(2, 0, 3),
        fac_simplicem(2, 1, 2),
        fac_simplicem(2, 1, 3),
        fac_simplicem(2, 2, 3)
    );

    /* Facies (2-simplices) */
    adde_simplices(
        complexus, 4,
        fac_simplicem(3, 0, 1, 2),
        fac_simplicem(3, 0, 1, 3),
        fac_simplicem(3, 0, 2, 3),
        fac_simplicem(3, 1, 2, 3)
    );
}

static void
construe_limitem_octahedri(ComplexusSimplicialis *complexus)
{
    initia_complexum(complexus);

    /* 6 vertices */
    for (int i = 0; i < 6; i++)
        adde_simplices(complexus, 1, fac_simplicem(1, i));

    /* 12 latera */
    int latera[][2] = {
        {0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 2}, {1, 4}, {2, 3}, {3, 4}, {1, 5}, {2, 5}, {3, 5}, {4, 5}
    };
    for (int i = 0; i < 12; i++)
        adde_simplices(
            complexus, 1,
            fac_simplicem(2, latera[i][0], latera[i][1])
        );

    /* 8 facies */
    int facies[][3] = {
        {0, 1, 2}, {0, 2, 3}, {0, 3, 4}, {0, 4, 1}, {5, 1, 2}, {5, 2, 3}, {5, 3, 4}, {5, 4, 1}
    };
    for (int i = 0; i < 8; i++)
        adde_simplices(
            complexus, 1,
            fac_simplicem(3, facies[i][0], facies[i][1], facies[i][2])
        );
}

/* ===== Programma principale ===== */

int
main(void)
{
    ComplexusSimplicialis complexus;
    int f_vector[MAXIMA_DIMENSIO + 1];

    printf("Complexus Simpliciales et Characteristica Euleri\n\n");

    /* Limes tetrahedri */
    construe_limitem_tetrahedri(&complexus);
    printf(
        "=== %s ===\n",
        forma_textum("Limes Tetrahedri (%d simplices)", complexus.numerositas)
    );

    printf("Simplices:\n");
    for (int i = 0; i < complexus.numerositas; i++) {
        printf("  ");
        scribe_simplicem(&complexus.simplices[i]);
        printf("  (dim %d)\n", complexus.simplices[i].numerositas - 1);
    }

    computa_f_vectorem(&complexus, f_vector, MAXIMA_DIMENSIO);
    printf("f-vector: (");
    for (int d = 0; d <= complexus.dimensio_maxima; d++) {
        if (d > 0)
            printf(", ");
        printf("f%d=%d", d, f_vector[d]);
    }
    printf(")\n");
    int chi = characteristica_euleri(f_vector, complexus.dimensio_maxima);
    printf("chi = %d (expectatum: 2 pro sphaera)\n\n", chi);

    /* Limes octahedri */
    construe_limitem_octahedri(&complexus);
    printf(
        "=== %s ===\n",
        forma_textum("Limes Octahedri (%d simplices)", complexus.numerositas)
    );

    computa_f_vectorem(&complexus, f_vector, MAXIMA_DIMENSIO);
    printf("f-vector: (");
    for (int d = 0; d <= complexus.dimensio_maxima; d++) {
        if (d > 0)
            printf(", ");
        printf("f%d=%d", d, f_vector[d]);
    }
    printf(")\n");
    chi = characteristica_euleri(f_vector, complexus.dimensio_maxima);
    printf("chi = %d (expectatum: 2 pro sphaera)\n\n", chi);

    /* Relatio Dehn-Sommerville */
    printf("Relationes f-vectoris:\n");
    printf("  Tetrahedron: f0 - f1 + f2 = V - E + F\n");
    construe_limitem_tetrahedri(&complexus);
    computa_f_vectorem(&complexus, f_vector, MAXIMA_DIMENSIO);
    printf(
        "  f0=%d, f1=%d, f2=%d => %d - %d + %d = %d\n",
        f_vector[0], f_vector[1], f_vector[2],
        f_vector[0], f_vector[1], f_vector[2],
        f_vector[0] - f_vector[1] + f_vector[2]
    );

    printf("\nComputatio perfecta.\n");
    return 0;
}
