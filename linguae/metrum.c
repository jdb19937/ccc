/*
 * metrum.c — Scansio Hexametri Latini
 *
 * Analyzat lineam hexametri dactylici per syllabas et signat
 * syllabam longam (—) aut brevem (u) secundum regulas quantitatis
 * classicae.  Hexameter dactylicus sex habet pedes, unusquisque
 * dactylus (— u u) aut spondeus (— —).
 *
 * Cruciatus compilatoris: campi bitorum, machinae statuum cum goto,
 * tabulae staticae constantes, indicatores functionum, litteralia
 * composita ut argumenta.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINEA     512
#define MAX_SYLLABAE  128

/* ===== Classificatio charactarum ===== */

static const char vocales[] = "aeiouyAEIOUY";

/*
    * diphthongi Latini qui semper longi sunt.
 * Tabula terminatur cum NULL.
 */
static const char *const diphthongi[] = {
    "ae", "au", "oe", "ei", "eu", "ui", NULL
};

static int est_vocalis(char c) { return strchr(vocales, c) != NULL; }

/* Est c consonans (non vocalis, non spatium, non littera non-Latina)? */
static int est_consonans(char c)
{
    if (!isalpha((unsigned char)c))
        return 0;
    return !est_vocalis(c);
}

/* ===== Structura syllabae cum campis bitorum ===== */

/*
    * proprietates syllabae in campis bitorum.
 * Longitudo: 0=brevis, 1=longa, 2=indeterminata.
 * Origo: 0=natura (vocalis longa vel diphthongus),
 *        1=positio (duae consonantes sequentes).
 */
typedef struct {
    unsigned int longitudo     : 2;
    unsigned int origo         : 1;
    unsigned int est_diphthong : 1;
    unsigned int initium_verbi : 1;
    unsigned int positus_start : 10;  /* offset in linea */
    unsigned int longitudo_str : 5;   /* longitudo syllabae in char */
} Syllaba;

/* ===== Partitor linearum in syllabas ===== */

/*
 * Divisor syllabarum: simplex heuristica.
 * Una syllaba = una vocalis (aut diphthongus) cum consonantibus adiunctis.
 * Pro hoc exemplo, accipimus regulam: nucleus vocalicus
 * + consonantes praecedentes (omnes ante nucleum sequens).
 */
static int
divide_syllabas(const char *linea, Syllaba *ss, int max)
{
    int n = 0;
    int i = 0;
    int lon = (int)strlen(linea);
    int novum_verbum = 1;

    while (i < lon && n < max) {
        /* praetermitte non-alphabeta */
        if (!isalpha((unsigned char)linea[i])) {
            if (linea[i] == ' ' || linea[i] == '\t')
                novum_verbum = 1;
            i++;
            continue;
        }

        int start = i;
        /* consume consonantes initiales */
        while (i < lon && est_consonans(linea[i]))
            i++;
        /* nucleus vocalicus — una vocalis aut diphthongus */
        int est_diph = 0;
        if (i + 1 < lon && est_vocalis(linea[i]) && est_vocalis(linea[i+1])) {
            char par[3] = { (char)tolower((unsigned char)linea[i]),
                            (char)tolower((unsigned char)linea[i+1]), 0 };
            for (int d = 0; diphthongi[d]; d++) {
                if (strcmp(par, diphthongi[d]) == 0) {
                    est_diph = 1;
                    break;
                }
            }
        }
        if (est_diph)
            i += 2;
        else if (i < lon && est_vocalis(linea[i]))
            i++;
        else
            break;  /* nullum nucleum — terminamus */

        /*
         * Consonantes finales: accipe usque ad unam consonantem ante
         * sequens nucleum.  Simplificatio.
         */
        int fines = i;
        while (fines < lon && est_consonans(linea[fines]))
            fines++;
        /* si sequitur vocalis, cede unam consonantem ad syllabam sequentem */
        if (fines < lon && est_vocalis(linea[fines]) && fines > i)
            fines--;
        /* sed duae consonantes ante vocalem: prima ad hanc, secunda ad sequentem */
        i = fines;

        /* scribe syllabam */
        ss[n] = (Syllaba){
            .longitudo = 2,  /* indeterminata adhuc */
            .origo = 0,
            .est_diphthong = (unsigned)est_diph,
            .initium_verbi = (unsigned)novum_verbum,
            .positus_start = (unsigned)start,
            .longitudo_str = (unsigned)(i - start),
        };
        n++;
        novum_verbum = 0;
    }
    return n;
}

/* ===== Determinatio longitudinis ===== */

/*
 * Regulae longitudinis syllabae:
 * 1. Diphthongus — longa per naturam.
 * 2. Vocalis sequitur duas consonantes (aut consonantem geminatam,
 *    aut consonantem finale syllabae) — longa per positionem.
 * 3. Aliter — indeterminata (defaltus: brevis).
 * Nota: hoc simplificat regulas verae quantitatis Latinae.
 */
static void
determina_longitudines(const char *linea, Syllaba *ss, int n)
{
    int lon = (int)strlen(linea);
    for (int i = 0; i < n; i++) {
        if (ss[i].est_diphthong) {
            ss[i].longitudo = 1;
            ss[i].origo = 0;  /* natura */
            continue;
        }
        /* fines syllabae */
        int fin = ss[i].positus_start + ss[i].longitudo_str;
        int cons_sequentes = 0;
        /* consonantes post hanc syllabam sed ante vocalem proximam */
        int j = fin;
        while (j < lon && !est_vocalis(linea[j])
               && isalpha((unsigned char)linea[j])) {
            cons_sequentes++;
            j++;
        }
        if (cons_sequentes >= 2) {
            ss[i].longitudo = 1;
            ss[i].origo = 1;  /* positio */
        } else {
            /* Syllaba ultima lineae conventionaliter longa. */
            ss[i].longitudo = (i == n - 1) ? 1 : 0;
            ss[i].origo = 0;
        }
    }
}

/* ===== Monstrator scansionis ===== */

static void
monstra_scansionem(const char *linea, const Syllaba *ss, int n)
{
    printf("\n  Linea: %s\n", linea);
    printf("  Scans: ");

    /* scribe signa sub linea */
    int cur = 0;
    for (int i = 0; i < n; i++) {
        /* impletura spatiis usque ad start */
        while (cur < (int)ss[i].positus_start) {
            putchar(' ');
            cur++;
        }
        char sig = (ss[i].longitudo == 1) ? '-' : 'u';
        putchar(sig);
        /* impletura usque ad finem syllabae */
        for (unsigned k = 1; k < ss[i].longitudo_str; k++) {
            putchar(' ');
        }
        cur = ss[i].positus_start + ss[i].longitudo_str;
    }
    putchar('\n');

    /* summarium */
    int longae = 0, breves = 0;
    for (int i = 0; i < n; i++) {
        if (ss[i].longitudo == 1) longae++;
        else breves++;
    }
    printf("  Syllabae: %d (longae %d, breves %d)\n", n, longae, breves);
}

/* ===== Exempla defalta ===== */

static const char *const exempla_defalta[] = {
    "arma virumque cano troiae qui primus ab oris",
    "italiam fato profugus lavinaque venit",
    "multa quoque et bello passus dum conderet urbem",
    "tityre tu patulae recubans sub tegmine fagi",
    NULL,
};

int
main(int argc, char *argv[])
{
    printf("Scansor Hexametri Latini\n");
    printf("========================\n");

    if (argc > 1) {
        Syllaba ss[MAX_SYLLABAE];
        int n = divide_syllabas(argv[1], ss, MAX_SYLLABAE);
        determina_longitudines(argv[1], ss, n);
        monstra_scansionem(argv[1], ss, n);
    } else {
        for (int i = 0; exempla_defalta[i]; i++) {
            Syllaba ss[MAX_SYLLABAE];
            int n = divide_syllabas(exempla_defalta[i], ss, MAX_SYLLABAE);
            determina_longitudines(exempla_defalta[i], ss, n);
            monstra_scansionem(exempla_defalta[i], ss, n);
        }
    }

    return 0;
}
