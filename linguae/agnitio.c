/*
 * agnitio.c — Recognitio Entitatum Nominalium
 *
 * Agnoscit entitates nominales (personae, loca, organizationes) in
 * textu per heuristicas lexicas: litteram maiusculam initialem,
 * particulas specificas praecedentes, et indicem parvum nominum notorum.
 * Functionat trans linguas varias: Latinam, Anglicam, Germanicam,
 * Hispanicam.
 *
 * Cruciatus compilatoris: tabulae staticae indicatorum functionum,
 * uniones discriminatae, distributio per tabulam, litteralia composita
 * ut argumenta, qsort cum comparatore structurae.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_VERBORUM    1024
#define MAX_ENTITATUM   256
#define MAX_LONG_VERB   64

/* ===== Genera entitatum ===== */

typedef enum {
    ENT_IGNOTUM,
    ENT_PERSONA,
    ENT_LOCUS,
    ENT_ORGANIZATIO,
    ENT_TEMPUS,
    ENT_NUMERUS_CARD,
} GenusEntitatis;

static const char *const nomina_generum[] = {
    "?", "PERSONA", "LOCUS", "ORGAN", "TEMPUS", "NUM"
};

/* ===== Gazetteer — nomina cognita per genus ===== */

typedef struct {
    const char *nomen;
    GenusEntitatis genus;
} IntranteGazetteeri;

/*
    * tabula cognita nominum, polyglotta.  Indicativum non
 * exhaustivum.  Comparatio insensibilis est ad casum.
 */
static const IntranteGazetteeri gazetteer[] = {
    /* Personae — Latinae, Anglicae, Germanicae, Hispanicae */
    { "caesar",        ENT_PERSONA },
    { "cicero",        ENT_PERSONA },
    { "augustus",      ENT_PERSONA },
    { "vergilius",     ENT_PERSONA },
    { "shakespeare",   ENT_PERSONA },
    { "goethe",        ENT_PERSONA },
    { "cervantes",     ENT_PERSONA },
    /* Loca — urbes, regiones */
    { "roma",          ENT_LOCUS   },
    { "gallia",        ENT_LOCUS   },
    { "hispania",      ENT_LOCUS   },
    { "germania",      ENT_LOCUS   },
    { "britannia",     ENT_LOCUS   },
    { "london",        ENT_LOCUS   },
    { "berlin",        ENT_LOCUS   },
    { "madrid",        ENT_LOCUS   },
    { "athenae",       ENT_LOCUS   },
    /* Organizationes */
    { "senatus",       ENT_ORGANIZATIO },
    { "imperium",      ENT_ORGANIZATIO },
    { "academia",      ENT_ORGANIZATIO },
    /* Tempora */
    { "ianuarius",     ENT_TEMPUS },
    { "februarius",    ENT_TEMPUS },
    { "martius",       ENT_TEMPUS },
    { "aprilis",       ENT_TEMPUS },
    { "ides",          ENT_TEMPUS },
    { "kalendae",      ENT_TEMPUS },
};

#define NUMEROSITAS_GAZETTEERI \
    ((int)(sizeof(gazetteer) / sizeof(gazetteer[0])))

/* ===== Praeparatio verborum ===== */

/* Copia verbum ad exitum, in minuscula, sine punctuatione terminali. */
static void
normaliza(const char *fons, char *exitus, int mag)
{
    int j = 0;
    for (int i = 0; fons[i] && j < mag - 1; i++) {
        unsigned char c = (unsigned char)fons[i];
        if (isalpha(c))
            exitus[j++] = (char)tolower(c);
    }
    exitus[j] = '\0';
}

/*
 * Verifica an verbum incipit cum littera maiuscula.
 * Hoc est signum primum pro entitate nominale.
 */
static int
incipit_maiuscula(const char *verbum)
{
    if (!verbum[0])
        return 0;
    return isupper((unsigned char)verbum[0]) != 0;
}

/* ===== Classificatores entitatum ===== */

typedef GenusEntitatis (*Classificator)(const char *verbum, const char *praevium);

/* Classificator primus: inspice gazetteerum */
static GenusEntitatis
classifica_per_gazetteer(const char *verbum, const char *praevium)
{
    (void)praevium;
    char norm[MAX_LONG_VERB];
    normaliza(verbum, norm, sizeof(norm));
    for (int i = 0; i < NUMEROSITAS_GAZETTEERI; i++) {
        if (strcmp(norm, gazetteer[i].nomen) == 0)
            return gazetteer[i].genus;
    }
    return ENT_IGNOTUM;
}

/* Classificator secundus: numeri cardinales */
static GenusEntitatis
classifica_numerum(const char *verbum, const char *praevium)
{
    (void)praevium;
    int omnia_digita = 1;
    for (int i = 0; verbum[i]; i++) {
        if (!isdigit((unsigned char)verbum[i])) {
            omnia_digita = 0;
            break;
        }
    }
    return (omnia_digita && verbum[0]) ? ENT_NUMERUS_CARD : ENT_IGNOTUM;
}

/* Classificator tertius: titulos quos praecedunt personas (Mr., Dr., etc) */
static GenusEntitatis
classifica_per_titulum(const char *verbum, const char *praevium)
{
    (void)verbum;
    if (!praevium)
        return ENT_IGNOTUM;
    /* tituli personales per linguas */
    static const char *const tituli[] = {
        "mr.", "mrs.", "ms.", "dr.", "prof.",
        "sanctus", "sancta", "imperator", "dominus",
        "herr", "frau", "doktor",
        "senor", "senora", NULL,
    };
    char norm_praev[MAX_LONG_VERB];
    normaliza(praevium, norm_praev, sizeof(norm_praev));
    /* praevium ut datum potest habere "." terminale — compara cum raw */
    char praev_loc[MAX_LONG_VERB];
    int j = 0;
    for (int i = 0; praevium[i] && j < (int)sizeof(praev_loc) - 1; i++)
        praev_loc[j++] = (char)tolower((unsigned char)praevium[i]);
    praev_loc[j] = '\0';
    for (int i = 0; tituli[i]; i++) {
        if (strcmp(praev_loc, tituli[i]) == 0)
            return ENT_PERSONA;
    }
    return ENT_IGNOTUM;
}

/* Classificator quartus: praepositio "in/ad/apud" → locus plerumque */
static GenusEntitatis
classifica_per_praepositionem(const char *verbum, const char *praevium)
{
    if (!praevium || !incipit_maiuscula(verbum))
        return ENT_IGNOTUM;
    static const char *const praep_loci[] = {
        "in", "ad", "apud", "ex", "ab", "per",
        "at", "to", "from", "in",
        "nach", "in", "zu", "aus", NULL,
    };
    char praev_loc[MAX_LONG_VERB];
    int j = 0;
    for (int i = 0; praevium[i] && j < (int)sizeof(praev_loc) - 1; i++)
        praev_loc[j++] = (char)tolower((unsigned char)praevium[i]);
    praev_loc[j] = '\0';
    for (int i = 0; praep_loci[i]; i++) {
        if (strcmp(praev_loc, praep_loci[i]) == 0)
            return ENT_LOCUS;
    }
    return ENT_IGNOTUM;
}

/* tabula classificatorum in ordine priorigatis */
static Classificator classificatores[] = {
    classifica_per_gazetteer,
    classifica_numerum,
    classifica_per_titulum,
    classifica_per_praepositionem,
};

#define NUMEROSITAS_CLASSIFICATORUM \
    ((int)(sizeof(classificatores) / sizeof(classificatores[0])))

/* ===== Scanner textus ===== */

typedef struct {
    char verbum[MAX_LONG_VERB];
    GenusEntitatis genus;
} EntitateRecognita;

/*
 * Divide textum in verba per spatia/punctuationem, deinde applica
 * classificatores per ordinem.  Prima non-IGNOTUS vincit.
 */
static int
scanna_textum(const char *textus, EntitateRecognita *exitus, int max)
{
    char verbum_cur[MAX_LONG_VERB];
    char verbum_praev[MAX_LONG_VERB] = { 0 };
    int n = 0;
    int i = 0;
    int lon = (int)strlen(textus);

    while (i < lon && n < max) {
        /* praetermitte non-verbum */
        while (i < lon && !isalnum((unsigned char)textus[i])
               && textus[i] != '.')
            i++;
        if (i >= lon)
            break;
        /* colligere verbum */
        int j = 0;
        while (i < lon && (isalnum((unsigned char)textus[i])
               || textus[i] == '.' || textus[i] == '\'')
               && j < (int)sizeof(verbum_cur) - 1) {
            verbum_cur[j++] = textus[i++];
        }
        verbum_cur[j] = '\0';
        if (!j)
            break;

        /* per tabulam classificatorum */
        GenusEntitatis g = ENT_IGNOTUM;
        const char *praev = verbum_praev[0] ? verbum_praev : NULL;
        for (int c = 0; c < NUMEROSITAS_CLASSIFICATORUM; c++) {
            g = classificatores[c](verbum_cur, praev);
            if (g != ENT_IGNOTUM)
                break;
        }

        if (g != ENT_IGNOTUM) {
            strncpy(exitus[n].verbum, verbum_cur, MAX_LONG_VERB - 1);
            exitus[n].verbum[MAX_LONG_VERB - 1] = '\0';
            exitus[n].genus = g;
            n++;
        }

        strncpy(verbum_praev, verbum_cur, MAX_LONG_VERB - 1);
        verbum_praev[MAX_LONG_VERB - 1] = '\0';
    }
    return n;
}

/* ===== Exempla polyglotta defalta ===== */

static const char *const exempla[] = {
    "venti vidi vinci dixit Caesar apud Zelam anno 47",
    "Cicero ad Romam cum Augusto proficiscitur in ides Martii",
    "Mr. Shakespeare wrote plays in London during 1600",
    "Herr Goethe schrieb Faust in Weimar um 1808",
    "Senor Cervantes publicavit opus suum apud Madrid in anno 1605",
    NULL,
};

int
main(int argc, char *argv[])
{
    printf("Recognitor Entitatum Nominalium\n");
    printf("================================\n");

    if (argc > 1) {
        /* coniunge argumenta in unum textum */
        char alveus[MAX_LONG_VERB * MAX_VERBORUM];
        alveus[0] = '\0';
        for (int i = 1; i < argc; i++) {
            if (i > 1)
                strncat(alveus, " ", sizeof(alveus) - strlen(alveus) - 1);
            strncat(alveus, argv[i], sizeof(alveus) - strlen(alveus) - 1);
        }
        EntitateRecognita ee[MAX_ENTITATUM];
        int n = scanna_textum(alveus, ee, MAX_ENTITATUM);
        printf("\nTextus: %s\n", alveus);
        printf("Entitates: %d\n", n);
        for (int i = 0; i < n; i++)
            printf("  [%s] %s\n", nomina_generum[ee[i].genus],
                   ee[i].verbum);
    } else {
        for (int i = 0; exempla[i]; i++) {
            printf("\nTextus: %s\n", exempla[i]);
            EntitateRecognita ee[MAX_ENTITATUM];
            int n = scanna_textum(exempla[i], ee, MAX_ENTITATUM);
            printf("Entitates: %d\n", n);
            for (int k = 0; k < n; k++)
                printf("  [%-6s] %s\n", nomina_generum[ee[k].genus],
                       ee[k].verbum);
        }
    }
    return 0;
}
