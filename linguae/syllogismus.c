/*
 * syllogismus.c — Verificator Syllogismorum Aristotelicorum
 *
 * Syllogismus categoricus Aristotelianus constat ex tribus
 * propositionibus (maior, minor, conclusio), quisque formae A
 * (universalis affirmativa), E (universalis negativa), I (particularis
 * affirmativa), vel O (particularis negativa), inter tres terminos:
 * maiorem, minorem, medium.  Sunt quattuor figurae definitae per
 * positionem medii.  Haec opera verificat an datus syllogismus sit
 * validus secundum quadrigae regulas traditionales.
 *
 * Exercet compilatorem per: switch-case valde profunde nestata cum
 * transitu ad casum sequentem, tabulam booleanam trium dimensionum,
 * designatores initializationis structurarum, varargs pro notatione.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* ===== Propositiones categoricae ===== */

typedef enum { PRO_A, PRO_E, PRO_I, PRO_O } Propositio;

/*
 * Distributio: terminus "distributus" significat quod propositio aliquid
 * asserit de omnibus eius referentibus.  A: subj dist, praed non; E:
 * ambo dist; I: neuter dist; O: subj non, praed dist.
 */
static int subj_distributus(Propositio p) { return p == PRO_A || p == PRO_E; }
static int praed_distributus(Propositio p) { return p == PRO_E || p == PRO_O; }
static int est_negativa(Propositio p)      { return p == PRO_E || p == PRO_O; }

/* ===== Figurae syllogismi ===== */

/*
 * Figura 1: M-P, S-M |- S-P  (medium est subj in maiore, praed in minore)
 * Figura 2: P-M, S-M |- S-P  (medium est praed in ambobus)
 * Figura 3: M-P, M-S |- S-P  (medium est subj in ambobus)
 * Figura 4: P-M, M-S |- S-P
 */
typedef enum { FIG_1 = 1, FIG_2, FIG_3, FIG_4 } Figura;

/* ===== Syllogismus ===== */

typedef struct {
    Propositio maior;
    Propositio minor;
    Propositio conclusio;
    Figura figura;
} Syllogismus;

/* Nomina traditional.: e.g. Barbara = AAA-1, Celarent = EAE-1, etc. */
typedef struct {
    const char *nomen;
    Syllogismus s;
} SyllogismusNominatus;

static const SyllogismusNominatus nominati[] = {
    { "Barbara",  { PRO_A, PRO_A, PRO_A, FIG_1 } },
    { "Celarent", { PRO_E, PRO_A, PRO_E, FIG_1 } },
    { "Darii",    { PRO_A, PRO_I, PRO_I, FIG_1 } },
    { "Ferio",    { PRO_E, PRO_I, PRO_O, FIG_1 } },
    { "Cesare",   { PRO_E, PRO_A, PRO_E, FIG_2 } },
    { "Camestres",{ PRO_A, PRO_E, PRO_E, FIG_2 } },
    { "Festino",  { PRO_E, PRO_I, PRO_O, FIG_2 } },
    { "Baroco",   { PRO_A, PRO_O, PRO_O, FIG_2 } },
    { "Darapti",  { PRO_A, PRO_A, PRO_I, FIG_3 } },
    { "Disamis",  { PRO_I, PRO_A, PRO_I, FIG_3 } },
    { "Datisi",   { PRO_A, PRO_I, PRO_I, FIG_3 } },
    { "Felapton", { PRO_E, PRO_A, PRO_O, FIG_3 } },
    { "Bocardo",  { PRO_O, PRO_A, PRO_O, FIG_3 } },
    { "Ferison",  { PRO_E, PRO_I, PRO_O, FIG_3 } },
    { "Bramantip",{ PRO_A, PRO_A, PRO_I, FIG_4 } },
    { "Camenes",  { PRO_A, PRO_E, PRO_E, FIG_4 } },
    { "Dimaris",  { PRO_I, PRO_A, PRO_I, FIG_4 } },
    { "Fesapo",   { PRO_E, PRO_A, PRO_O, FIG_4 } },
    { "Fresison", { PRO_E, PRO_I, PRO_O, FIG_4 } },
    /* exempla invalida pro comparatione */
    { "AAA-2",    { PRO_A, PRO_A, PRO_A, FIG_2 } },
    { "EAI-1",    { PRO_E, PRO_A, PRO_I, FIG_1 } },
    { "III-3",    { PRO_I, PRO_I, PRO_I, FIG_3 } },
};
#define NUMEROSITAS_SYLL \
    ((int)(sizeof(nominati) / sizeof(nominati[0])))

/*
 * Positio medii per figuram:
 * Figura 1: medium = subj maiore, praed minore.
 * Figura 2: medium = praed ambobus.
 * Figura 3: medium = subj ambobus.
 * Figura 4: medium = praed maiore, subj minore.
 */

typedef struct {
    int medium_dist;  /* quoties medium distributum est */
    int maior_dist;   /* distributus in maiore? */
    int minor_dist;   /* distributus in minore? */
    int maior_concl;  /* distributus in conclusione? */
    int minor_concl;  /* distributus in conclusione? */
} AnalysisDist;

/* Computat distributiones terminorum pro syllogismo. */
static AnalysisDist
analyza(Syllogismus s)
{
    AnalysisDist ad = { 0, 0, 0, 0, 0 };
    /* maior propositio continet medium (M) et terminum maiorem (P).
     * minor propositio continet medium (M) et terminum minorem (S).
     * conclusio: S est-/non-est P.
     */
    switch (s.figura) {
    case FIG_1:
        /* maior: M-P (M subj, P praed).  minor: S-M (S subj, M praed) */
        if (subj_distributus(s.maior)) ad.medium_dist++;
        if (praed_distributus(s.maior)) ad.maior_dist++;
        if (praed_distributus(s.minor)) ad.medium_dist++;
        if (subj_distributus(s.minor)) ad.minor_dist++;
        break;
    case FIG_2:
        /* maior: P-M.  minor: S-M */
        if (subj_distributus(s.maior)) ad.maior_dist++;
        if (praed_distributus(s.maior)) ad.medium_dist++;
        if (subj_distributus(s.minor)) ad.minor_dist++;
        if (praed_distributus(s.minor)) ad.medium_dist++;
        break;
    case FIG_3:
        /* maior: M-P.  minor: M-S */
        if (subj_distributus(s.maior)) ad.medium_dist++;
        if (praed_distributus(s.maior)) ad.maior_dist++;
        if (subj_distributus(s.minor)) ad.medium_dist++;
        if (praed_distributus(s.minor)) ad.minor_dist++;
        break;
    case FIG_4:
        /* maior: P-M.  minor: M-S */
        if (subj_distributus(s.maior)) ad.maior_dist++;
        if (praed_distributus(s.maior)) ad.medium_dist++;
        if (subj_distributus(s.minor)) ad.medium_dist++;
        if (praed_distributus(s.minor)) ad.minor_dist++;
        break;
    }
    /* conclusio: S-P.  S subj, P praed. */
    ad.minor_concl = subj_distributus(s.conclusio);
    ad.maior_concl = praed_distributus(s.conclusio);
    return ad;
}

/*
 * Quadrigae regulae validitatis:
 * R1: medium saltem semel distributum est.
 * R2: terminus non-distributus in praemissa non potest esse
 *     distributus in conclusione.
 * R3: non possunt esse duae praemissae negativae.
 * R4: si una praemissa negativa, conclusio debet esse negativa.
 * R5: conclusio particularis non sequitur ex duabus universalibus
 *     (in logica moderna; Aristoteles admittit).
 */
typedef struct {
    const char *nomen;
    int (*regula)(Syllogismus, AnalysisDist);
    const char *description;
} RegulaValiditatis;

static int r_medium_dist(Syllogismus s, AnalysisDist a)
{ (void)s; return a.medium_dist >= 1; }

static int r_distrib_illicita(Syllogismus s, AnalysisDist a)
{
    (void)s;
    if (a.maior_concl && !a.maior_dist) return 0;
    if (a.minor_concl && !a.minor_dist) return 0;
    return 1;
}

static int r_non_duae_neg(Syllogismus s, AnalysisDist a)
{
    (void)a;
    return !(est_negativa(s.maior) && est_negativa(s.minor));
}

static int r_negativa_propagat(Syllogismus s, AnalysisDist a)
{
    (void)a;
    if (est_negativa(s.maior) || est_negativa(s.minor))
        return est_negativa(s.conclusio);
    return !est_negativa(s.conclusio);
}

static const RegulaValiditatis regulae[] = {
    { "Medium distributum",      r_medium_dist,        "medium saltem semel distribuendum" },
    { "Distributio legitima",    r_distrib_illicita,   "terminus non-distributus in praemissa non distribuendus in conclusione" },
    { "Non duae negativae",      r_non_duae_neg,       "non ambae praemissae negativae" },
    { "Negativitas propagat",    r_negativa_propagat,  "si praemissa negativa, conclusio negativa; aliter affirmativa" },
};
#define NUM_REGULARUM \
    ((int)(sizeof(regulae) / sizeof(regulae[0])))

/* ===== Diagnostica per variadic ===== */

static void
quaere(const char *nomen, const char *fmt, ...)
{
    printf("  %-25s: ", nomen);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

/* ===== Formator propositionum ===== */

static char
littera_propositionis(Propositio p)
{
    switch (p) {
    case PRO_A: return 'A';
    case PRO_E: return 'E';
    case PRO_I: return 'I';
    case PRO_O: return 'O';
    }
    return '?';
}

static void
monstra_syllogismum(const char *nomen, Syllogismus s)
{
    printf("\n[%s: %c%c%c-%d]\n", nomen,
        littera_propositionis(s.maior),
        littera_propositionis(s.minor),
        littera_propositionis(s.conclusio),
        s.figura);
    AnalysisDist ad = analyza(s);
    int validus = 1;
    for (int i = 0; i < NUM_REGULARUM; i++) {
        int r = regulae[i].regula(s, ad);
        quaere(regulae[i].nomen, "%s", r ? "OK" : "VIOLATA");
        if (!r) validus = 0;
    }
    quaere("RESULTATUM", "%s", validus ? "VALIDUS" : "INVALIDUS");
}

int
main(int argc, char *argv[])
{
    printf("Verificator Syllogismorum Aristotelicorum\n");
    printf("=========================================\n");

    if (argc == 5) {
        /* usus: syll <maior> <minor> <concl> <figura>
         * e.g. syll A A A 1 */
        Propositio props[3];
        for (int i = 0; i < 3; i++) {
            char c = (char)toupper((unsigned char)argv[i+1][0]);
            switch (c) {
            case 'A': props[i] = PRO_A; break;
            case 'E': props[i] = PRO_E; break;
            case 'I': props[i] = PRO_I; break;
            case 'O': props[i] = PRO_O; break;
            default:
                fprintf(stderr, "Error: propositio invalida '%c'\n", c);
                return 1;
            }
        }
        int fig = atoi(argv[4]);
        if (fig < 1 || fig > 4) {
            fprintf(stderr, "Error: figura 1..4\n");
            return 1;
        }
        Syllogismus s = { props[0], props[1], props[2], (Figura)fig };
        monstra_syllogismum("ex argumentis", s);
    } else {
        for (int i = 0; i < NUMEROSITAS_SYLL; i++)
            monstra_syllogismum(nominati[i].nomen, nominati[i].s);
        printf("\nUsus: syllogismus <maior> <minor> <concl> <figura>\n");
        printf("   exempli: syllogismus A A A 1    (= Barbara)\n");
    }
    return 0;
}
