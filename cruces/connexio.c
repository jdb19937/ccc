/*
 * connexio.c — Connexitas Graphorum et Componentes
 *
 * Invenit componentes connexas graphi per explorationem in profundum.
 * Pontes et vertices articulationis invenit.
 * Cruciatus compilatoris: goto pro liberatione copiarum (cleanup pattern),
 * goto in plicis nestificatis (multi-level break), plures labels
 * in eadem functione, goto ante et post declarationes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXIMI_VERTICES 64

/* ===== Typi ===== */

typedef struct {
    int numerositas_verticium;
    int numerositas_laterum;
    unsigned char matrice[MAXIMI_VERTICES][MAXIMI_VERTICES];
    const char *nomen;
} Graphus;

typedef struct {
    int componentis[MAXIMI_VERTICES];  /* indici componentis pro quoque vertice */
    int numerositas_componentium;
} ResultatumConnexitatis;

/* ===== Exploratio in profundum per acervum (non recursione) ===== */

/*
 * DFS iterativa cum goto pro liberatione copiarum.
 * Pattern: allocatio -> operatio -> goto liberatio.
 * Si allocatio fallit, saltat ad 'liberatio' label.
 */
static int
explora_componentem(
    const Graphus *graphus, int initium,
    int *visitata, int indicis_componentis
) {
    int magnitudo   = 0;
    int *acervus    = NULL;
    int apex_acervi = 0;

    /* Allocatio */
    acervus = malloc((size_t)graphus->numerositas_verticium * sizeof(int));
    if (acervus == NULL)
        goto liberatio_erroris;

    /* Initium explorationis */
    acervus[apex_acervi++] = initium;
    visitata[initium] = indicis_componentis;
    magnitudo = 1;

    while (apex_acervi > 0) {
        int vertex = acervus[--apex_acervi];

        for (int vicinus = 0; vicinus < graphus->numerositas_verticium; vicinus++) {
            if (graphus->matrice[vertex][vicinus] && visitata[vicinus] < 0) {
                visitata[vicinus]      = indicis_componentis;
                acervus[apex_acervi++] = vicinus;
                magnitudo++;
            }
        }
    }

    /* Successus — liberatio normalis */
    free(acervus);
    return magnitudo;

    /* Tractatus erroris per goto */
liberatio_erroris:
    free(acervus);
    return -1;
}

/*
 * Invenit omnes componentes connexas.
 * Goto in plicis nestificatis: multi-level break.
 */
static ResultatumConnexitatis
inveni_componentes(const Graphus *graphus)
{
    ResultatumConnexitatis resultatum;
    memset(resultatum.componentis, -1, sizeof(resultatum.componentis));
    resultatum.numerositas_componentium = 0;

    for (int v = 0; v < graphus->numerositas_verticium; v++) {
        if (resultatum.componentis[v] < 0) {
            int magnitudo = explora_componentem(
                graphus, v, resultatum.componentis,
                resultatum.numerositas_componentium
            );
            if (magnitudo < 0)
                break;  /* error allocandi */
            resultatum.numerositas_componentium++;
        }
    }

    return resultatum;
}

/* ===== Pontes: latera quorum remotio disconnexionem causat ===== */

/*
 * Invenit pontes graphi.
 * Usus goto ad exitum ex plicis nestificatis tripliciter
 * (goto pro multi-level break).
 */
static int
inveni_pontes(const Graphus *graphus)
{
    int pontes = 0;
    Graphus graphus_mutilatus;
    int visitata[MAXIMI_VERTICES];

    /* Pro quoque latere: remove et verifica connexitatem */
    for (int u = 0; u < graphus->numerositas_verticium; u++) {
        for (int v = u + 1; v < graphus->numerositas_verticium; v++) {
            if (!graphus->matrice[u][v])
                continue;

            /* Copia graphi sine hoc latere */
            memcpy(&graphus_mutilatus, graphus, sizeof(Graphus));
            graphus_mutilatus.matrice[u][v] = 0;
            graphus_mutilatus.matrice[v][u] = 0;

            /* Verifica an u et v adhuc connexi sunt */
            memset(visitata, 0, sizeof(visitata));
            int *acervus = malloc((size_t)graphus->numerositas_verticium * sizeof(int));
            if (acervus == NULL)
                goto finis_pontium;

            int apex        = 0;
            acervus[apex++] = u;
            visitata[u]     = 1;
            int invenit_v   = 0;

            while (apex > 0 && !invenit_v) {
                int vertex = acervus[--apex];
                for (int w = 0; w < graphus_mutilatus.numerositas_verticium; w++) {
                    if (graphus_mutilatus.matrice[vertex][w] && !visitata[w]) {
                        if (w == v) {
                            invenit_v = 1;
                            /*
                             * goto ad exitum e plica nestificata:
                             * multi-level break per goto.
                             */
                            goto latus_non_pons;
                        }
                        visitata[w]     = 1;
                        acervus[apex++] = w;
                    }
                }
            }

            if (!invenit_v) {
                printf("  Pons: (%d, %d)\n", u, v);
                pontes++;
            }

latus_non_pons:
            free(acervus);
        }
    }

finis_pontium:
    return pontes;
}

/* ===== Constructio graphorum ===== */

/*
 * Addit latus ad graphum.
 * Goto ante declarationes — licet in C99 (sed variabilis
 * non est in ambitu ante declarationem).
 */
static void
adde_latus(Graphus *graphus, int u, int v)
{
    if (
        u < 0 || u >= graphus->numerositas_verticium ||
        v < 0 || v >= graphus->numerositas_verticium
    )
        goto finis_addendi;

    /* Declaratio post label — C99 non permittit label ante declarationem directe */
    {
        int iam_existit = graphus->matrice[u][v];
        if (!iam_existit) {
            graphus->matrice[u][v] = 1;
            graphus->matrice[v][u] = 1;
            graphus->numerositas_laterum++;
        }
    }

finis_addendi:
    return;
}

static Graphus
crea_graphum_petersen(void)
{
    Graphus g;
    memset(&g, 0, sizeof(g));
    g.numerositas_verticium = 10;
    g.nomen = "Graphus Petersen";

    /* Pentagonum exterius: 0-1-2-3-4-0 */
    for (int i = 0; i < 5; i++)
        adde_latus(&g, i, (i + 1) % 5);

    /* Pentagramma interius: 5-7-9-6-8-5 */
    for (int i = 0; i < 5; i++)
        adde_latus(&g, 5 + i, 5 + (i + 2) % 5);

    /* Radii: i — (i+5) */
    for (int i = 0; i < 5; i++)
        adde_latus(&g, i, i + 5);

    return g;
}

static Graphus
crea_graphum_disconnexum(void)
{
    Graphus g;
    memset(&g, 0, sizeof(g));
    g.numerositas_verticium = 8;
    g.nomen = "Graphus disconnexus";

    /* Componens 1: triangulum 0-1-2 */
    adde_latus(&g, 0, 1);
    adde_latus(&g, 1, 2);
    adde_latus(&g, 2, 0);

    /* Componens 2: latus 3-4 */
    adde_latus(&g, 3, 4);

    /* Componens 3: triangulum 5-6-7 cum ponte */
    adde_latus(&g, 5, 6);
    adde_latus(&g, 6, 7);

    /* Vertex isolatus: nemo (omnes habent latera) */
    return g;
}

static Graphus
crea_arborem(void)
{
    Graphus g;
    memset(&g, 0, sizeof(g));
    g.numerositas_verticium = 7;
    g.nomen = "Arbor (omnia latera sunt pontes)";

    adde_latus(&g, 0, 1);
    adde_latus(&g, 0, 2);
    adde_latus(&g, 1, 3);
    adde_latus(&g, 1, 4);
    adde_latus(&g, 2, 5);
    adde_latus(&g, 2, 6);

    return g;
}

/* ===== Programma principale ===== */

static void
analysa_graphum(Graphus *graphus)
{
    printf("\n=== %s ===\n", graphus->nomen);
    printf(
        "V = %d, E = %d\n",
        graphus->numerositas_verticium,
        graphus->numerositas_laterum
    );

    ResultatumConnexitatis res = inveni_componentes(graphus);
    printf("Componentes connexae: %d\n", res.numerositas_componentium);

    for (int c = 0; c < res.numerositas_componentium; c++) {
        printf("  Componens %d:", c);
        for (int v = 0; v < graphus->numerositas_verticium; v++) {
            if (res.componentis[v] == c)
                printf(" %d", v);
        }
        putchar('\n');
    }

    /* Characteristica Euleri graphi: V - E + C (C = componentium) */
    int chi = graphus->numerositas_verticium
        - graphus->numerositas_laterum
        + res.numerositas_componentium;
    printf(
        "V - E + C = %d - %d + %d = %d\n",
        graphus->numerositas_verticium,
        graphus->numerositas_laterum,
        res.numerositas_componentium,
        chi
    );

    printf("Pontes (latera critici):\n");
    int numerositas_pontium = inveni_pontes(graphus);
    if (numerositas_pontium == 0)
        printf("  (nulli pontes)\n");
    else
        printf("  Numerositas pontium: %d\n", numerositas_pontium);
}

int
main(void)
{
    printf("Connexitas Graphorum\n");

    Graphus petersen = crea_graphum_petersen();
    analysa_graphum(&petersen);

    Graphus disconnexus = crea_graphum_disconnexum();
    analysa_graphum(&disconnexus);

    Graphus arbor = crea_arborem();
    analysa_graphum(&arbor);

    printf("\nComputatio perfecta.\n");
    return 0;
}
