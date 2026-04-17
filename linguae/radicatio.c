/*
 * radicatio.c — Reductor Verborum ad Radicem
 *
 * Reducit verbum ad radicem per ablationem terminationum notarum.
 * Tractat linguas plures per tabulas terminationum distinctas;
 * linguam per argumentum specificari potest (la, en, de, es) aut
 * heuristice agnosci per comparationem longissimae terminationis
 * applicabilis.
 *
 * Cruciatus compilatoris: uniones discriminatae, tabulae indicatorum
 * ad const, distributio per linguam, reditus structurae ex functione,
 * functiones variadicae, campi bitorum pro vexillis.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_VERBI  128

/* ===== Linguae sustentae ===== */

typedef enum { LAT, ANG, GER, HIS, LIN_IGNOTA } Lingua;

static const char *const nomina_linguarum[] = {
    "Latina", "Anglica", "Germanica", "Hispanica", "ignota"
};

/* ===== Tabulae terminationum per linguam ===== */

/*
    * unusquisque modus stemming habet suam listam terminationum.
 * Longiores termiantiones probentur prius.  Ordo significat!
 */

static const char *const terminationes_lat[] = {
    "ibus", "arum", "orum", "abat", "ebat", "ibat",
    "are", "ere", "ire", "tis", "mus", "nt",
    "us", "um", "em", "is", "es", "ae", "am", "ur",
    "o", "a", "e", "i", "m", "s", "t",
    NULL
};

static const char *const terminationes_ang[] = {
    "ational", "tional", "ization", "ations", "ement",
    "ation", "ingly", "izing", "iness", "fully", "ously",
    "able", "ible", "less", "ness", "ment", "ism",
    "ing", "ies", "ied", "est",
    "ly", "al", "er", "ed", "es", "ic",
    "s", NULL
};

static const char *const terminationes_ger[] = {
    "ungen", "keiten", "ieren", "erung", "ender",
    "ungs", "keit", "chen", "lein", "heit", "isch",
    "end", "lich", "ung", "bar", "ern",
    "en", "er", "es", "em", "nd", "st",
    "e", "t", NULL
};

static const char *const terminationes_his[] = {
    "amientos", "imientos", "aciones", "iciones",
    "amiento", "imiento", "ables", "ibles",
    "acion", "icion", "ando", "endo",
    "aban", "ian", "mente",
    "ar", "er", "ir", "as", "es", "os",
    "a", "e", "o", "s", NULL
};

/* tabula linguarum → tabulis terminationum */
static const char *const *const terminationes_per_linguam[] = {
    terminationes_lat,
    terminationes_ang,
    terminationes_ger,
    terminationes_his,
    NULL,
};

/* ===== Structura resultati radicationis ===== */

typedef struct {
    char radix[MAX_VERBI];
    char terminatio[16];
    Lingua lingua;
    int   mutatum;
} Radicatio;

/* ===== Agnitio linguae per heuristicam ===== */

/*
 * Si lingua non est data, heuristice inveniamus per terminationes
 * frequentes.  Simplex: quae lingua habet terminationem quae "fit"
 * verbo maximam reductionem?
 */
static Lingua
agnosce_linguam(const char *verbum)
{
    int lon = (int)strlen(verbum);
    int melior_reduct = 0;
    Lingua melior_lingua = LIN_IGNOTA;

    for (int l = 0; l < 4; l++) {
        const char *const *terms = terminationes_per_linguam[l];
        for (int t = 0; terms[t]; t++) {
            int term_lon = (int)strlen(terms[t]);
            if (term_lon >= lon)
                continue;
            if (strcmp(verbum + lon - term_lon, terms[t]) == 0) {
                if (term_lon > melior_reduct) {
                    melior_reduct = term_lon;
                    melior_lingua = (Lingua)l;
                }
                break;  /* prima matchata — proxima lingua */
            }
        }
    }
    return melior_lingua;
}

/* ===== Radicatio ipsa ===== */

/*
 * Reduce verbum ad radicem per tabulam terminationum linguae.
 * Retine radicem minimum longitudine 2 char.
 */
static Radicatio
radica(const char *verbum, Lingua lingua)
{
    Radicatio res;
    res.radix[0] = '\0';
    res.terminatio[0] = '\0';
    res.lingua = lingua;
    res.mutatum = 0;

    if (lingua >= LIN_IGNOTA) {
        /* non agnita — redde verbum intactum */
        snprintf(res.radix, MAX_VERBI, "%s", verbum);
        return res;
    }

    /* normaliza ad minuscula */
    char norm[MAX_VERBI];
    int i = 0;
    for (; verbum[i] && i < MAX_VERBI - 1; i++)
        norm[i] = (char)tolower((unsigned char)verbum[i]);
    norm[i] = '\0';
    int lon = i;

    const char *const *terms = terminationes_per_linguam[lingua];
    for (int t = 0; terms[t]; t++) {
        int term_lon = (int)strlen(terms[t]);
        if (term_lon >= lon - 1)
            continue;  /* radix debet esse >= 2 char */
        if (strcmp(norm + lon - term_lon, terms[t]) == 0) {
            /* match invenit */
            memcpy(res.radix, norm, (size_t)(lon - term_lon));
            res.radix[lon - term_lon] = '\0';
            snprintf(res.terminatio, sizeof(res.terminatio), "%s", terms[t]);
            res.mutatum = 1;
            return res;
        }
    }
    /* nulla terminatio applicatur */
    snprintf(res.radix, MAX_VERBI, "%s", norm);
    return res;
}

/* ===== Monstrator ===== */

static void
monstra_radicationem(const char *verbum, Lingua lingua)
{
    Lingua l = (lingua == LIN_IGNOTA) ? agnosce_linguam(verbum) : lingua;
    Radicatio r = radica(verbum, l);
    printf("  %-20s  ->  %-15s", verbum, r.radix);
    if (r.mutatum) {
        printf("  (-%s, %s)", r.terminatio, nomina_linguarum[r.lingua]);
    } else {
        printf("  (%s)", nomina_linguarum[r.lingua]);
    }
    putchar('\n');
}

/* ===== Exempla polyglotta ===== */

static const struct {
    const char *verbum;
    Lingua lingua;
} exempla_defalta[] = {
    /* Latina */
    { "amabat",       LAT },
    { "dominorum",    LAT },
    { "legendarum",   LAT },
    { "currentibus",  LAT },
    /* Anglica */
    { "running",      ANG },
    { "operational",  ANG },
    { "happiness",    ANG },
    { "quickly",      ANG },
    /* Germanica */
    { "arbeitend",    GER },
    { "Schoenheit",   GER },
    { "Maedchen",     GER },
    { "freundlich",   GER },
    /* Hispanica */
    { "corriendo",    HIS },
    { "hablaban",     HIS },
    { "naciones",     HIS },
    { "rapidamente",  HIS },
    /* heuristicae */
    { "venti",        LIN_IGNOTA },
    { "vinci",        LIN_IGNOTA },
    { "invention",    LIN_IGNOTA },
};

#define NUMEROSITAS_EXEMPLORUM \
    ((int)(sizeof(exempla_defalta) / sizeof(exempla_defalta[0])))

/* ===== Parsor linguae ex argumento ===== */

static Lingua
parsa_linguam(const char *codex)
{
    if (strcmp(codex, "la") == 0) return LAT;
    if (strcmp(codex, "en") == 0) return ANG;
    if (strcmp(codex, "de") == 0) return GER;
    if (strcmp(codex, "es") == 0) return HIS;
    if (strcmp(codex, "auto") == 0) return LIN_IGNOTA;
    return LIN_IGNOTA;
}

int
main(int argc, char *argv[])
{
    printf("Radicator Verborum Polyglottus\n");
    printf("==============================\n");

    if (argc >= 3) {
        /* radicatio <lingua> <verbum> [verbum...] */
        Lingua l = parsa_linguam(argv[1]);
        for (int i = 2; i < argc; i++)
            monstra_radicationem(argv[i], l);
    } else if (argc == 2) {
        /* radicatio <verbum> — heuristice lingua */
        monstra_radicationem(argv[1], LIN_IGNOTA);
    } else {
        for (int i = 0; i < NUMEROSITAS_EXEMPLORUM; i++)
            monstra_radicationem(exempla_defalta[i].verbum,
                                 exempla_defalta[i].lingua);
        printf("\nUsus: radicatio [la|en|de|es|auto] verbum...\n");
    }
    return 0;
}
