/*
 * euleri.c — Genera Superficierum et Characteristica Euleri
 *
 * Computat genus et characteristicam Euleri superficierum clausarum
 * ex triangulatione data. Tractat errores per setjmp/longjmp.
 * Cruciatus compilatoris: setjmp/longjmp pro saltu non-locali,
 * variabiles 'volatile' in contextu setjmp (C99 7.13.2.1),
 * fluxus controli complexus cum pluribus punctis saltus.
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

/* ===== Codices errorum ===== */

#define ERROR_ALLOCATIO   1
#define ERROR_TOPOLOGIA   2
#define ERROR_TRIANGULATIO 3

/* Contextus saltus globalis pro tractatu errorum */
static jmp_buf contextus_erroris;

/* ===== Typi ===== */

typedef struct {
    int vertex[3];
} Triangulum;

typedef struct {
    const char *nomen;
    int numerositas_verticium;
    int numerositas_triangulorum;
    const Triangulum *triangula;
    int characteristica_expectata;
    int genus_expectatum;
} Superficies;

/* ===== Computatio characteristicae Euleri ===== */

/*
 * Computat numerum laterum unicorum ex triangulatione.
 * Variabiles 'volatile' — mandatum C99 7.13.2.1:
 * Post longjmp, solae variabiles 'volatile' habent valores definitos
 * si inter setjmp et longjmp mutatae sunt.
 */
static int
computa_latera(
    int numerositas_triangulorum,
    const Triangulum *triangula,
    int numerositas_verticium
) {
    /*
     * Matrice adiacentiae utimur ad latera unica numeranda.
     * 'volatile' quia post longjmp valor definitus esse debet.
     */
    volatile int latera_inventa = 0;
    int n = numerositas_verticium;

    /* Allocatio — si fallit, saltum facimus */
    unsigned char *matrice = calloc(
        (size_t)n * (size_t)n,
        sizeof(unsigned char)
    );
    if (matrice == NULL)
        longjmp(contextus_erroris, ERROR_ALLOCATIO);

    for (int t = 0; t < numerositas_triangulorum; t++) {
        int v0 = triangula[t].vertex[0];
        int v1 = triangula[t].vertex[1];
        int v2 = triangula[t].vertex[2];

        /* Verificatio limitum */
        if (
            v0 < 0 || v0 >= n || v1 < 0 || v1 >= n ||
            v2 < 0 || v2 >= n
        ) {
            free(matrice);
            longjmp(contextus_erroris, ERROR_TRIANGULATIO);
        }

        /* Signa latera (utraque directione) */
        if (!matrice[v0 * n + v1]) {
            matrice[v0 * n + v1] = 1;
            matrice[v1 * n + v0] = 1;
            latera_inventa++;
        }
        if (!matrice[v1 * n + v2]) {
            matrice[v1 * n + v2] = 1;
            matrice[v2 * n + v1] = 1;
            latera_inventa++;
        }
        if (!matrice[v2 * n + v0]) {
            matrice[v2 * n + v0] = 1;
            matrice[v0 * n + v2] = 1;
            latera_inventa++;
        }
    }

    free(matrice);
    return latera_inventa;
}

/*
 * Computat characteristicam Euleri: chi = V - E + F
 * ubi F = numerositas triangulorum (quisque triangulus est facies).
 */
static int
characteristica_euleri(const Superficies *sup)
{
    int latera = computa_latera(
        sup->numerositas_triangulorum,
        sup->triangula,
        sup->numerositas_verticium
    );
    return sup->numerositas_verticium
        - latera
        + sup->numerositas_triangulorum;
}

/*
 * Genus ex characteristica: chi = 2 - 2g (superficies orientabilis)
 * ergo g = (2 - chi) / 2
 */
static int
genus_ex_characteristica(int chi)
{
    if ((2 - chi) % 2 != 0)
        longjmp(contextus_erroris, ERROR_TOPOLOGIA);
    return (2 - chi) / 2;
}

/* ===== Data: triangulationes superficierum notarum ===== */

/* Tetrahedron = sphaera triangulata (genus 0, chi = 2) */
static const Triangulum tetrahedron[] = {
    {{0, 1, 2}}, {{0, 1, 3}}, {{0, 2, 3}}, {{1, 2, 3}}
};

/* Octahedron = sphaera triangulata (genus 0, chi = 2) */
static const Triangulum octahedron[] = {
    {{0, 1, 2}}, {{0, 2, 3}}, {{0, 3, 4}}, {{0, 4, 1}},
    {{5, 1, 2}}, {{5, 2, 3}}, {{5, 3, 4}}, {{5, 4, 1}}
};

/*
 * Torus minimalis — 14 triangula, 7 vertices
 * Triangulatio Moebii toroidalis
 * (vertices: 0-6, chi = 0, genus = 1)
 */
static const Triangulum torus[] = {
    {{0, 1, 3}}, {{1, 3, 4}}, {{1, 2, 4}}, {{2, 4, 5}},
    {{2, 0, 5}}, {{0, 5, 3}}, {{3, 4, 6}}, {{4, 6, 0}},
    {{4, 5, 0}}, {{5, 0, 1}}, {{5, 6, 1}}, {{6, 1, 2}},
    {{6, 2, 3}}, {{6, 3, 0}}
};

/* Tabula superficierum */
static const Superficies superficies[] = {
    {
        "Tetrahedron (Sphaera)",
        4, 4, tetrahedron,
        2, 0
    },
    {
        "Octahedron (Sphaera)",
        6, 8, octahedron,
        2, 0
    },
    {
        "Torus minimalis",
        7, 14, torus,
        0, 1
    },
};

#define NUMEROSITAS_SUPERFICIERUM \
    ((int)(sizeof(superficies) / sizeof(superficies[0])))

int
main(void)
{
    /*
     * setjmp: servat contextum executionis.
     * Si longjmp invocatur, executio huc redit cum valore non-nulo.
     * Variabilis 'volatile' necessaria est quia inter setjmp et
     * longjmp mutatur.
     */
    volatile int index_superficiei = 0;
    int codex_erroris = setjmp(contextus_erroris);

    if (codex_erroris != 0) {
        /* Huc pervenimus per longjmp — error accidit */
        const char *nuntium =
            (codex_erroris == ERROR_ALLOCATIO) ? "allocatio memoriae fallit" :
            (codex_erroris == ERROR_TOPOLOGIA) ? "topologia invalida" :
            (codex_erroris == ERROR_TRIANGULATIO) ? "triangulatio invalida" :
            "error ignotus";
        fprintf(
            stderr, "Error in superficie %d: %s\n",
            (int)index_superficiei, nuntium
        );
        return codex_erroris;
    }

    printf("Genera Superficierum Clausarum\n");
    printf("Formula: chi = V - E + F = 2 - 2g\n\n");
    printf(
        "%-25s  %3s  %3s  %3s  %5s  %5s  %8s\n",
        "Superficies", "V", "E", "F", "chi", "genus", "status"
    );
    printf(
        "%-25s  %3s  %3s  %3s  %5s  %5s  %8s\n",
        "-------------------------", "---", "---", "---",
        "-----", "-----", "--------"
    );

    int errores = 0;
    for (int i = 0; i < NUMEROSITAS_SUPERFICIERUM; i++) {
        index_superficiei      = i;
        const Superficies *sup = &superficies[i];

        int chi   = characteristica_euleri(sup);
        int genus = genus_ex_characteristica(chi);

        int latera = computa_latera(
            sup->numerositas_triangulorum,
            sup->triangula,
            sup->numerositas_verticium
        );

        const char *status = "recte";
        if (
            chi != sup->characteristica_expectata ||
            genus != sup->genus_expectatum
        ) {
            status = "FALLIT";
            errores++;
        }

        printf(
            "%-25s  %3d  %3d  %3d  %5d  %5d  %8s\n",
            sup->nomen,
            sup->numerositas_verticium,
            latera,
            sup->numerositas_triangulorum,
            chi, genus, status
        );
    }

    /* Theorema classificationis */
    printf("\nTheorema classificationis superficierum clausarum orientabilium:\n");
    printf("  Genus 0: Sphaera S^2 (chi = 2)\n");
    printf("  Genus 1: Torus T^2 (chi = 0)\n");
    printf("  Genus g: Summa connexa g tororum (chi = 2 - 2g)\n");

    printf("\nCharacteristicae Euleri pro generibus 0..10:\n");
    for (int g = 0; g <= 10; g++) {
        printf("  g = %2d: chi = %d\n", g, 2 - 2 * g);
    }

    if (errores > 0) {
        fprintf(stderr, "Error: %d verificiones falluerunt\n", errores);
        return 1;
    }

    printf("\nOmnes superficies recte computatae sunt.\n");
    return 0;
}
