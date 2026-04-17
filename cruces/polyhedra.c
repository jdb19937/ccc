/*
 * polyhedra.c — Polyhedra Platonica et Formula Euleri
 *
 * Verificat V - E + F = 2 pro quinque solidis Platonicis,
 * dualitatem explorat, et characteristicam Euleri computat.
 * Cruciatus compilatoris: designatores initializationis nestificati,
 * designatores indicis arietis [N], litteralia composita (compound
 * literals) ut argumenta functionum et in expressionibus.
 */

#include <stdio.h>

/* ===== Typi ===== */

typedef struct {
    int vertices;  /* V */
    int latera;    /* E */
    int facies;    /* F */
} Polyhedron;

typedef struct {
    char nomen[20];
    Polyhedron forma;
    int latera_faciei;   /* p: latera per faciem */
    int gradus_verticis; /* q: facies per verticem */
} SolidumPlatonicum;

/* ===== Designatores initializationis nestificati ===== */

/*
 * Designatores indicis [N] cum designatoribus membrorum .nomen
 * nestificati — compilator debet ordinem arbitrarium tractare.
 */
static const SolidumPlatonicum solida[] = {
    [4] = {
        .nomen = "Icosahedron",
        .forma = { .vertices = 12, .latera = 30, .facies = 20 },
        .latera_faciei = 3,
        .gradus_verticis = 5,
    },
    [0] = {
        .nomen = "Tetrahedron",
        .forma = { .vertices = 4, .latera = 6, .facies = 4 },
        .latera_faciei = 3,
        .gradus_verticis = 3,
    },
    [2] = {
        .nomen = "Octahedron",
        .forma = { .vertices = 6, .latera = 12, .facies = 8 },
        .latera_faciei = 3,
        .gradus_verticis = 4,
    },
    [1] = {
        .nomen = "Hexahedron",
        .forma = { .vertices = 8, .latera = 12, .facies = 6 },
        .latera_faciei = 4,
        .gradus_verticis = 3,
    },
    [3] = {
        .nomen = "Dodecahedron",
        .forma = { .vertices = 20, .latera = 30, .facies = 12 },
        .latera_faciei = 5,
        .gradus_verticis = 3,
    },
};

#define NUMEROSITAS_SOLIDORUM \
    ((int)(sizeof(solida) / sizeof(solida[0])))

/* ===== Functiones cum litteralibus compositis ===== */

/*
 * Acceptat Polyhedron per valorem — caller potest
 * litterale compositum transmittere.
 */
static int
characteristica_euleri(Polyhedron ph)
{
    return ph.vertices - ph.latera + ph.facies;
}

/*
 * Acceptat indicatorem ad Polyhedron — caller potest
 * indicatorem ad litterale compositum transmittere.
 */
static int
verifica_relationes(
    const char *nomen, const Polyhedron *ph,
    int p, int q
) {
    int errores = 0;

    /* Relatio: p * F = 2 * E */
    if (p * ph->facies != 2 * ph->latera) {
        printf(
            "  %s: p*F = %d, 2*E = %d — NON CONCORDANT\n",
            nomen, p * ph->facies, 2 * ph->latera
        );
        errores++;
    }

    /* Relatio: q * V = 2 * E */
    if (q * ph->vertices != 2 * ph->latera) {
        printf(
            "  %s: q*V = %d, 2*E = %d — NON CONCORDANT\n",
            nomen, q * ph->vertices, 2 * ph->latera
        );
        errores++;
    }

    return errores;
}

/* Constructio dualis: V <-> F, p <-> q */
static Polyhedron
duale(Polyhedron originale)
{
    return (Polyhedron){
        .vertices = originale.facies,
        .latera = originale.latera,
        .facies = originale.vertices,
    };
}

int
main(void)
{
    int errores = 0;

    printf("Polyhedra Platonica — Formula Euleri: V - E + F = 2\n\n");
    printf(
        "%-14s  %3s  %3s  %3s  %3s  %3s  %5s\n",
        "Nomen", "V", "E", "F", "p", "q", "V-E+F"
    );
    printf(
        "%-14s  %3s  %3s  %3s  %3s  %3s  %5s\n",
        "--------------", "---", "---", "---", "---", "---", "-----"
    );

    for (int i = 0; i < NUMEROSITAS_SOLIDORUM; i++) {
        const SolidumPlatonicum *s = &solida[i];

        /*
         * Litterale compositum ut argumentum functionis —
         * creatur in acervo calloris et transmittitur per valorem.
         */
        int chi = characteristica_euleri(
            (Polyhedron){
            .vertices = s->forma.vertices,
            .latera = s->forma.latera,
            .facies = s->forma.facies
            }
        );

        printf(
            "%-14s  %3d  %3d  %3d  %3d  %3d  %5d",
            s->nomen,
            s->forma.vertices, s->forma.latera, s->forma.facies,
            s->latera_faciei, s->gradus_verticis,
            chi
        );

        if (chi != 2) {
            printf("  FALLACIA!");
            errores++;
        }
        putchar('\n');

        /*
         * Indicatorem ad litterale compositum transmittimus.
         * Litterale compositum habet durationem automaticam
         * usque ad finem ambitus includentis.
         */
        errores += verifica_relationes(
            s->nomen,
            &(Polyhedron){ s->forma.vertices, s->forma.latera, s->forma.facies },
            s->latera_faciei,
            s->gradus_verticis
        );
    }

    /* Dualitas: dualis icosahedri est dodecahedron */
    printf("\nDualitas polyhedrorum:\n");
    for (int i = 0; i < NUMEROSITAS_SOLIDORUM; i++) {
        Polyhedron d   = duale(solida[i].forma);
        int chi_dualis = characteristica_euleri(d);
        printf(
            "  Dualis %s: V=%d E=%d F=%d, chi=%d\n",
            solida[i].nomen, d.vertices, d.latera, d.facies,
            chi_dualis
        );
        if (chi_dualis != 2)
            errores++;
    }

    /*
     * Litterale compositum in expressione conditionali.
     * Ternarius cum litteralibus compositis ut operandis.
     */
    int n_vertices = 4;
    Polyhedron simplex = (n_vertices == 4)
        ? (Polyhedron){ 4, 6, 4 }
        : (Polyhedron){ 0, 0, 0 };
    printf(
        "\nSimplex (tetrahedron): V=%d E=%d F=%d, chi=%d\n",
        simplex.vertices, simplex.latera, simplex.facies,
        characteristica_euleri(simplex)
    );

    if (errores > 0) {
        fprintf(stderr, "Error: %d fallaciae inventae\n", errores);
        return 1;
    }

    printf("\nOmnia polyhedra formulam Euleri satisfaciunt.\n");
    return 0;
}
