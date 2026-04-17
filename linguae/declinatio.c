/*
 * declinatio.c — Declinatio Nominum Latinorum
 *
 * Declinat nomen Latinum per casus omnes (nominativus, genitivus,
 * dativus, accusativus, ablativus, vocativus) in numero singulari
 * et plurali.  Agnoscit quinque declinationes classicas.
 *
 * Cruciatus compilatoris: tabulae magnae staticae cum designatoribus
 * initializationis, enumerationes cum valoribus implicitis, indicatores
 * functionum in structuris, campi bitorum pro proprietatibus, compound
 * literals ut argumenta. */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Enumerationes casuum et numerorum ===== */

typedef enum {
    CASUS_NOMINATIVUS,
    CASUS_GENITIVUS,
    CASUS_DATIVUS,
    CASUS_ACCUSATIVUS,
    CASUS_ABLATIVUS,
    CASUS_VOCATIVUS,
    CASUS_NUMERUS  /* terminator */
} Casus;

typedef enum { SINGULARIS, PLURALIS } Numerus;

/* campi bitorum pro proprietatibus nominis —
 * declinatio (1..5), genus (m/f/n), irregulare. */
typedef struct {
    unsigned int declinatio     : 3;  /* 1..5 */
    unsigned int genus          : 2;  /* 0=m, 1=f, 2=n */
    unsigned int irregulare     : 1;
    unsigned int neutrum_plural_a : 1;
} ProprietatesNominis;

/* ===== Tabula terminationum per declinationem/casum/numerum ===== */

/*
 * Terminationes pro quinque declinationibus.  Structura trinaria:
 * [declinatio-1][numerus][casus].  Casus in ordine: nom, gen, dat, acc, abl, voc.
 */
static const char *const terminationes[5][2][6] = {
    /* declinatio prima: rosa, rosae */
    {
        { "a",  "ae",   "ae",   "am",   "a",    "a"  },
        { "ae", "arum", "is",   "as",   "is",   "ae" },
    },
    /* declinatio secunda: dominus, domini — masculinum standardum */
    {
        { "us", "i",    "o",    "um",   "o",    "e"  },
        { "i",  "orum", "is",   "os",   "is",   "i"  },
    },
    /* declinatio tertia: rex, regis */
    {
        { "",   "is",   "i",    "em",   "e",    ""   },
        { "es", "um",   "ibus", "es",   "ibus", "es" },
    },
    /* declinatio quarta: manus, manus */
    {
        { "us", "us",   "ui",   "um",   "u",    "us" },
        { "us", "uum",  "ibus", "us",   "ibus", "us" },
    },
    /* declinatio quinta: dies, diei */
    {
        { "es", "ei",   "ei",   "em",   "e",    "es" },
        { "es", "erum", "ebus", "es",   "ebus", "es" },
    },
};

/* ===== Tabula exemplorum nominum ===== */

typedef struct {
    const char *radix;   /* radix sine terminatione */
    const char *nomen;   /* forma nominativi pro monstrando */
    ProprietatesNominis prop;
} ExemplumNominis;

static const ExemplumNominis exempla[] = {
    { "ros",    "rosa",     { 1, 1, 0, 0 } },  /* rosa, f */
    { "domin",  "dominus",  { 2, 0, 0, 0 } },  /* dominus, m */
    { "bell",   "bellum",   { 2, 2, 0, 0 } },  /* bellum, n */
    { "reg",    "rex",      { 3, 0, 1, 0 } },  /* rex, m, irreg */
    { "man",    "manus",    { 4, 1, 0, 0 } },  /* manus, f */
    { "di",     "dies",     { 5, 0, 0, 0 } },  /* dies, m */
};

#define NUMEROSITAS_EXEMPLORUM \
    ((int)(sizeof(exempla) / sizeof(exempla[0])))

/* ===== Nomina Latina casuum pro monstrando ===== */

static const char *const nomina_casuum[] = {
    "nom.", "gen.", "dat.", "acc.", "abl.", "voc.",
};

/* ===== Compositio formae ===== */

/*
 * Applicat terminationem ad radicem.  Tractat casum specialem
 * nominativi tertiae declinationis (terminatio vacua).
 */
static void
compone_formam(
    char *exitus, int magnitudo,
    const char *radix, const char *nomen,
    Casus casus, Numerus numerus,
    ProprietatesNominis prop
) {
    int decl_idx = prop.declinatio - 1;
    const char *term = terminationes[decl_idx][numerus][casus];

    /*
     * pro nominibus neutris declinationis secundae, accusativus
     * sequitur nominativum.  Hic simpliciter: bellum, belli, bello, bellum...
     */
    if (prop.genus == 2 && prop.declinatio == 2) {
        if (casus == CASUS_NOMINATIVUS || casus == CASUS_ACCUSATIVUS
            || casus == CASUS_VOCATIVUS)
            term = (numerus == SINGULARIS) ? "um" : "a";
    }

    /*
     * Casus specialis: nominativus tertiae declinationis irregularis —
     * utere forma data (e.g., "rex" non "reg").
     */
    if (prop.irregulare && casus == CASUS_NOMINATIVUS
        && numerus == SINGULARIS) {
        snprintf(exitus, magnitudo, "%s", nomen);
        return;
    }

    snprintf(exitus, magnitudo, "%s%s", radix, term);
}

/* ===== Monstrator paradigmatis ===== */

/*
 * Monstrat paradigma completum (omnes casus, ambos numeros)
 * pro exemplo dato.
 */
static void
monstra_paradigma(const ExemplumNominis *ex)
{
    static const char *const genera_nomina[] = { "masc.", "fem.", "neut." };

    printf("\n== %s (decl. %u, %s) ==\n",
        ex->nomen, ex->prop.declinatio, genera_nomina[ex->prop.genus]);
    printf("  %-5s  %-15s  %-15s\n", "casus", "singularis", "pluralis");
    printf("  %-5s  %-15s  %-15s\n", "-----", "----------", "--------");

    for (int c = 0; c < 6; c++) {
        char forma_sg[64], forma_pl[64];
        compone_formam(forma_sg, sizeof(forma_sg),
            ex->radix, ex->nomen, (Casus)c, SINGULARIS, ex->prop);
        compone_formam(forma_pl, sizeof(forma_pl),
            ex->radix, ex->nomen, (Casus)c, PLURALIS, ex->prop);
        printf("  %-5s  %-15s  %-15s\n",
            nomina_casuum[c], forma_sg, forma_pl);
    }
}

int
main(int argc, char *argv[])
{
    printf("Declinator Nominum Latinorum\n");
    printf("============================\n");

    if (argc > 1) {
        /* Monstra solum specificatum per indicem (1..N) */
        char *finis;
        long idx = strtol(argv[1], &finis, 10);
        if (*finis != '\0' || idx < 1 || idx > NUMEROSITAS_EXEMPLORUM) {
            fprintf(stderr, "Error: index invalidus '%s' (1..%d)\n",
                argv[1], NUMEROSITAS_EXEMPLORUM);
            return 1;
        }
        monstra_paradigma(&exempla[idx - 1]);
    } else {
        for (int i = 0; i < NUMEROSITAS_EXEMPLORUM; i++)
            monstra_paradigma(&exempla[i]);
    }

    return 0;
}
