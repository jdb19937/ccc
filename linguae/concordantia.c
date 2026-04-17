/*
 * concordantia.c — Concordantia et Frequentiae Verborum
 *
 * Aedificat concordantiam textus: numerat quoties quodque verbum
 * apparet, et proponit eventus in ordine per frequentiam decrescentem.
 * Supra monstrat contextum brevem KWIC.
 *
 * Usus algorithmos exercet: tabulam dispersionum cum collisionibus per
 * catenam, qsort cum comparatore structurali, dynamicam allocationem
 * cum realloc, macra cum coniunctione et stringificatione signorum.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TABULA_MAG   1024
#define MAX_VERBI    64
#define MAX_CONTEXT  80

/* ===== Nodus tabulae dispersionum ===== */

typedef struct Intrans {
    char   verbum[MAX_VERBI];
    int    frequentia;
    int    prima_positio;
    struct Intrans *proximus;
} Intrans;

/* ===== Tabula ===== */

static Intrans *tabula[TABULA_MAG];

static unsigned
dispersio(const char *s)
{
    unsigned h = 2166136261u;
    for (int i = 0; s[i]; i++)
        h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}

/* ===== Insertio vel incrementatio ===== */

static void
insere(const char *verbum, int positio)
{
    unsigned h = dispersio(verbum) % TABULA_MAG;
    for (Intrans *p = tabula[h]; p; p = p->proximus) {
        if (strcmp(p->verbum, verbum) == 0) {
            p->frequentia++;
            return;
        }
    }
    Intrans *n = malloc(sizeof(Intrans));
    if (!n) {
        fprintf(stderr, "Error: memoria exhausta\n");
        exit(1);
    }
    strncpy(n->verbum, verbum, MAX_VERBI - 1);
    n->verbum[MAX_VERBI - 1] = '\0';
    n->frequentia = 1;
    n->prima_positio = positio;
    n->proximus = tabula[h];
    tabula[h] = n;
}

/* ===== Normalizatio verbi — ad minuscula, solum alphabetica ===== */

static void
normaliza(const char *in, char *out, int mag)
{
    int j = 0;
    for (int i = 0; in[i] && j < mag - 1; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalpha(c) || (j > 0 && c == '-'))
            out[j++] = (char)tolower(c);
    }
    out[j] = '\0';
}

/* ===== Scanner textus ===== */

static void
scanna_textum(const char *textus)
{
    int i = 0, lon = (int)strlen(textus);
    while (i < lon) {
        while (i < lon && !isalpha((unsigned char)textus[i]))
            i++;
        if (i >= lon) break;
        int start = i;
        char buf[MAX_VERBI];
        int j = 0;
        while (i < lon && (isalpha((unsigned char)textus[i])
               || textus[i] == '-' || textus[i] == '\'')
               && j < MAX_VERBI - 1) {
            buf[j++] = textus[i++];
        }
        buf[j] = '\0';
        char norm[MAX_VERBI];
        normaliza(buf, norm, sizeof(norm));
        if (norm[0])
            insere(norm, start);
    }
}

/* ===== Collectio omnium intrantium in arietem pro ordinatione ===== */

static int
colligue(Intrans **arietum, int max)
{
    int n = 0;
    for (int h = 0; h < TABULA_MAG; h++) {
        for (Intrans *p = tabula[h]; p; p = p->proximus) {
            if (n < max)
                arietum[n++] = p;
        }
    }
    return n;
}

/* ===== Comparatores pro qsort ===== */

static int
compara_per_frequentiam(const void *a, const void *b)
{
    const Intrans *x = *(const Intrans *const *)a;
    const Intrans *y = *(const Intrans *const *)b;
    if (x->frequentia != y->frequentia)
        return y->frequentia - x->frequentia;
    return strcmp(x->verbum, y->verbum);
}

static int
compara_per_alphabetum(const void *a, const void *b)
{
    const Intrans *x = *(const Intrans *const *)a;
    const Intrans *y = *(const Intrans *const *)b;
    return strcmp(x->verbum, y->verbum);
}

/* ===== Extractor contextus (KWIC) ===== */

static void
scribe_contextum(const char *textus, int positio, int ampl)
{
    int lon = (int)strlen(textus);
    int s = positio - ampl;
    int e = positio + ampl;
    if (s < 0) s = 0;
    if (e > lon) e = lon;
    putchar('\"');
    if (s > 0) fputs("...", stdout);
    for (int i = s; i < e; i++) {
        char c = textus[i];
        putchar(c == '\n' ? ' ' : c);
    }
    if (e < lon) fputs("...", stdout);
    putchar('\"');
}

/* ===== Liberatio tabulae ===== */

static void
libera(void)
{
    for (int h = 0; h < TABULA_MAG; h++) {
        Intrans *p = tabula[h];
        while (p) {
            Intrans *n = p->proximus;
            free(p);
            p = n;
        }
        tabula[h] = NULL;
    }
}

/* ===== Exempla polyglotta ===== */

static const char *const exempla[] = {
    "gallia est omnis divisa in partes tres quarum unam incolunt belgae "
    "aliam aquitani tertiam qui ipsorum lingua celtae nostra galli "
    "appellantur hi omnes lingua institutis legibus inter se differunt "
    "gallos ab aquitanis garumna flumen a belgis matrona et sequana dividit",

    "the quick brown fox jumps over the lazy dog and the dog barks at "
    "the fox the fox runs fast and the dog chases the fox all around "
    "the big brown yard where the brown fox lives with the quick fox "
    "family",

    NULL,
};

int
main(int argc, char *argv[])
{
    printf("Concordantia Verborum\n");
    printf("=====================\n");

    const char *textus;
    char alveus[MAX_CONTEXT * MAX_CONTEXT * 2];

    if (argc > 1) {
        alveus[0] = '\0';
        for (int i = 1; i < argc; i++) {
            if (i > 1)
                strncat(alveus, " ",
                        sizeof(alveus) - strlen(alveus) - 1);
            strncat(alveus, argv[i],
                    sizeof(alveus) - strlen(alveus) - 1);
        }
        textus = alveus;
    } else {
        textus = exempla[0];
    }

    scanna_textum(textus);

    Intrans *vec[4096];
    int n = colligue(vec, 4096);

    printf("\nTextus habet %d verba unica.\n", n);

    qsort(vec, n, sizeof(vec[0]), compara_per_frequentiam);

    int top = n < 20 ? n : 20;
    printf("\nVerba frequentissima (top %d):\n", top);
    printf("  %-20s  %s\n", "verbum", "frequ.");
    for (int i = 0; i < top; i++)
        printf("  %-20s  %d\n", vec[i]->verbum, vec[i]->frequentia);

    qsort(vec, n, sizeof(vec[0]), compara_per_alphabetum);

    int max_kwic = n < 8 ? n : 8;
    printf("\nConcordantia (KWIC) primorum %d verborum per alphabetum:\n", max_kwic);
    for (int i = 0; i < max_kwic; i++) {
        printf("  %-15s ", vec[i]->verbum);
        scribe_contextum(textus, vec[i]->prima_positio, 20);
        putchar('\n');
    }

    libera();

    if (argc == 1 && exempla[1]) {
        printf("\n--- Exemplum secundum ---\n");
        scanna_textum(exempla[1]);
        int m = colligue(vec, 4096);
        qsort(vec, m, sizeof(vec[0]), compara_per_frequentiam);
        int t = m < 10 ? m : 10;
        for (int i = 0; i < t; i++)
            printf("  %-20s  %d\n", vec[i]->verbum, vec[i]->frequentia);
        libera();
    }

    return 0;
}
