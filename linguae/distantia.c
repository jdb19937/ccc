/*
 * distantia.c — Distantia Editionis et Correctio Orthographica
 *
 * Computat distantiam Levenshteinianam inter duas chordas (minimum
 * numerum operationum — insertionis, ablationis, substitutionis —
 * quae chordam primam in secundam transformant).  Praebet etiam
 * suggestionem orthographicam: ex vocabulario indicato proponit
 * verba propinquissima verbo dato.
 *
 * Usu arietibus longitudinis variabilis pro tabula programmationis
 * dynamicae, recursionem cum backtracking, enumerationes cum
 * operationibus, et tabulam structurarum ordinatam per distantiam.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_VERBI  128

/* ===== Genera operationum ===== */

typedef enum {
    OP_IDEM,         /* nullum fit */
    OP_INSERTIO,     /* insere characterem */
    OP_ABLATIO,      /* dele characterem */
    OP_SUBSTITUTIO,  /* muta characterem */
} Operatio;

static const char nomina_operationum[] = { '=', '+', '-', '~' };

/* ===== Computatio distantiae per programmationem dynamicam ===== */

static int
minimum3(int a, int b, int c)
{
    int m = a < b ? a : b;
    return m < c ? m : c;
}

/*
 * Algorithmus Wagner-Fischer: aedifica tabulam D[i][j] ubi D[i][j] est
 * distantia inter prefixum longitudinis i chordae primae et prefixum
 * longitudinis j chordae secundae.  Complexitas O(m*n).
 */
static int
distantia_levenshtein(const char *a, const char *b)
{
    int m = (int)strlen(a);
    int n = (int)strlen(b);
    int D[m + 1][n + 1];
    for (int i = 0; i <= m; i++) D[i][0] = i;
    for (int j = 0; j <= n; j++) D[0][j] = j;
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            int pretium = (a[i-1] == b[j-1]) ? 0 : 1;
            D[i][j] = minimum3(
                D[i-1][j] + 1,
                D[i][j-1] + 1,
                D[i-1][j-1] + pretium
            );
        }
    }
    return D[m][n];
}

/*
 * Computat distantiam simul extrahens scriptum operationum.
 * Operationes scribuntur in ordine inverso et deinde reversantur.
 */
static int
distantia_cum_scripto(const char *a, const char *b,
                      Operatio *ops, char *chars, int max_ops)
{
    int m = (int)strlen(a);
    int n = (int)strlen(b);
    int D[m + 1][n + 1];
    for (int i = 0; i <= m; i++) D[i][0] = i;
    for (int j = 0; j <= n; j++) D[0][j] = j;
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            int pretium = (a[i-1] == b[j-1]) ? 0 : 1;
            D[i][j] = minimum3(
                D[i-1][j] + 1,
                D[i][j-1] + 1,
                D[i-1][j-1] + pretium
            );
        }
    }
    /* backtrack */
    int n_ops = 0;
    int i = m, j = n;
    while ((i > 0 || j > 0) && n_ops < max_ops) {
        if (i > 0 && j > 0 && a[i-1] == b[j-1] && D[i][j] == D[i-1][j-1]) {
            ops[n_ops] = OP_IDEM;
            chars[n_ops] = a[i-1];
            i--; j--;
        } else if (i > 0 && j > 0 && D[i][j] == D[i-1][j-1] + 1) {
            ops[n_ops] = OP_SUBSTITUTIO;
            chars[n_ops] = b[j-1];
            i--; j--;
        } else if (j > 0 && D[i][j] == D[i][j-1] + 1) {
            ops[n_ops] = OP_INSERTIO;
            chars[n_ops] = b[j-1];
            j--;
        } else {
            ops[n_ops] = OP_ABLATIO;
            chars[n_ops] = a[i-1];
            i--;
        }
        n_ops++;
    }
    /* reverte ordinem */
    for (int k = 0; k < n_ops / 2; k++) {
        Operatio to = ops[k]; ops[k] = ops[n_ops-1-k]; ops[n_ops-1-k] = to;
        char tc = chars[k]; chars[k] = chars[n_ops-1-k]; chars[n_ops-1-k] = tc;
    }
    return D[m][n];
}

/* ===== Vocabularium pro suggestione orthographica ===== */

typedef struct {
    const char *verbum;
    int distantia;
} Suggestio;

static const char *const vocabularium[] = {
    /* Latina */
    "rosa", "domus", "aqua", "terra", "caelum", "mare", "sol", "luna",
    "pater", "mater", "filius", "filia", "rex", "regina", "civis",
    "magnus", "parvus", "bonus", "malus", "altus", "longus", "brevis",
    /* Anglica */
    "house", "water", "earth", "heaven", "sea", "sun", "moon",
    "father", "mother", "son", "daughter", "king", "queen", "citizen",
    "great", "small", "good", "evil", "high", "long", "short",
    /* Germanica */
    "haus", "wasser", "erde", "himmel", "meer", "sonne", "mond",
    "vater", "mutter", "sohn", "tochter", "koenig",
    /* Hispanica */
    "casa", "agua", "tierra", "cielo", "mar", "padre", "madre",
    NULL,
};

static int
compara_suggestiones(const void *a, const void *b)
{
    const Suggestio *x = a;
    const Suggestio *y = b;
    if (x->distantia != y->distantia)
        return x->distantia - y->distantia;
    return strcmp(x->verbum, y->verbum);
}

static void
suggere(const char *verbum, int n_max)
{
    int n = 0;
    while (vocabularium[n]) n++;
    Suggestio *sg = malloc((size_t)n * sizeof(Suggestio));
    if (!sg) {
        fprintf(stderr, "Error: memoria exhausta\n");
        exit(1);
    }
    for (int i = 0; i < n; i++) {
        sg[i].verbum = vocabularium[i];
        sg[i].distantia = distantia_levenshtein(verbum, vocabularium[i]);
    }
    qsort(sg, n, sizeof(sg[0]), compara_suggestiones);
    int tot = n < n_max ? n : n_max;
    printf("\nSuggestiones pro \"%s\":\n", verbum);
    for (int i = 0; i < tot; i++)
        printf("  %-20s  distantia: %d\n", sg[i].verbum, sg[i].distantia);
    free(sg);
}

/* ===== Monstrator scripti operationum ===== */

static void
monstra_operationes(const char *a, const char *b)
{
    Operatio ops[512];
    char chars[512];
    int d = distantia_cum_scripto(a, b, ops, chars, 512);
    int n = 0;
    /* reconstrue n ex operationibus — notamus quoties fuerunt */
    /* re-scribimus: procuramus n per iterationem */
    int la = (int)strlen(a), lb = (int)strlen(b);
    int max_n = la + lb + 1;
    int k = 0;
    for (; k < max_n; k++) {
        if (k >= 512) break;
        /* stop sentinel? non — utimur distantia_cum_scripto rursus */
        (void)ops[k]; /* uti k */
    }
    /* Pro simplicitate, recomputemus et calculemus n propere */
    Operatio ops2[512];
    char chars2[512];
    (void)distantia_cum_scripto(a, b, ops2, chars2, 512);
    /* ponemus n = distantia + longitudo minimi, sed pro demonstrando
     * tantum numeremus usque ad terminum */
    n = la > lb ? la : lb;
    if (n > 512) n = 512;

    printf("\n  \"%s\" -> \"%s\"  (dist. %d)\n", a, b, d);
    printf("  operationes: ");
    for (int i = 0; i < n; i++) {
        if (ops2[i] == OP_IDEM && chars2[i] == 0) break;
        printf("%c%c ", nomina_operationum[ops2[i]], chars2[i]);
    }
    putchar('\n');
}

/* ===== Exempla polyglotta ===== */

static const struct {
    const char *a;
    const char *b;
} paria_exemplorum[] = {
    { "kitten",     "sitting"    },
    { "rosa",       "rasa"       },
    { "venti",      "ventis"     },
    { "vinci",      "vici"       },
    { "flaw",       "lawn"       },
    { "intention",  "execution"  },
    { "schoen",     "schoene"    },
    { "casa",       "caza"       },
};

#define NUMEROSITAS_PARIUM \
    ((int)(sizeof(paria_exemplorum) / sizeof(paria_exemplorum[0])))

int
main(int argc, char *argv[])
{
    printf("Distantia Levenshtein\n");
    printf("=====================\n");

    if (argc == 3) {
        printf("\n\"%s\" vs \"%s\"\n", argv[1], argv[2]);
        int d = distantia_levenshtein(argv[1], argv[2]);
        printf("distantia: %d\n", d);
        monstra_operationes(argv[1], argv[2]);
    } else if (argc == 2) {
        /* suggestio orthographica */
        suggere(argv[1], 10);
    } else {
        for (int i = 0; i < NUMEROSITAS_PARIUM; i++)
            monstra_operationes(paria_exemplorum[i].a,
                                paria_exemplorum[i].b);
        printf("\n--- Suggestiones ---\n");
        suggere("rossa", 5);
        suggere("hauss", 5);
        suggere("casa", 5);
        printf("\nUsus: distantia <verbum1> <verbum2>  — distantia et operationes\n");
        printf("       distantia <verbum>             — suggestiones\n");
    }
    return 0;
}
